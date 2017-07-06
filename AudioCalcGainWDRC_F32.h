/*
 * AudioCalcGainWDRC_F32
 * 
 * Created: Chip Audette, Feb 2017
 * Purpose: This module calculates the gain needed for wide dynamic range compression.
 * Derived From: Core algorithm is from "WDRC_circuit"
 *     WDRC_circuit from CHAPRO from BTNRC: https://github.com/BTNRH/chapro
 *     As of Feb 2017, CHAPRO license is listed as "Creative Commons?"
 *          
 * This processes a single stream fo audio data (ie, it is mono)       
 *          
 * MIT License.  use at your own risk.
*/

#ifndef _AudioCalcGainWDRC_F32_h
#define _AudioCalcGainWDRC_F32_h

#include <arm_math.h> //ARM DSP extensions.  for speed!
#include <AudioStream_F32.h>
#include "BTNRH_WDRC_Types.h"



class AudioCalcGainWDRC_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  //GUI: shortName:calc_WDRCGain
  public:
    //constructors
    AudioCalcGainWDRC_F32(void) : AudioStream_F32(1, inputQueueArray_f32) { setDefaultValues(); };
	AudioCalcGainWDRC_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) { setDefaultValues(); };
	
    //here's the method that does all the work
    void update(void) {
      
      //get the input audio data block
      audio_block_f32_t *in_block = AudioStream_F32::receiveReadOnly_f32(); // must be the envelope!
      if (!in_block) return;
  
      //prepare an output data block
      audio_block_f32_t *out_block = AudioStream_F32::allocate_f32();
      if (!out_block) return;
      
      // ////////////////////// do the processing here!
      calcGainFromEnvelope(in_block->data, out_block->data, in_block->length);
      out_block->length = in_block->length; out_block->fs_Hz = in_block->fs_Hz;
      
      //transmit the block and be done
      AudioStream_F32::transmit(out_block);
      AudioStream_F32::release(out_block);
      AudioStream_F32::release(in_block);
      
    }
  
    void calcGainFromEnvelope(float *env, float *gain_out, const int n)  {
      //env = input, signal envelope (not the envelope of the power, but the envelope of the signal itslef)
      //gain = output, the gain in natural units (not power, not dB)
      //n = input, number of samples to process in each vector
  
      //prepare intermediate data block
      audio_block_f32_t *env_dB_block = AudioStream_F32::allocate_f32();
      if (!env_dB_block) return;
  
      //convert to dB and calibrate (via maxdB)
      for (int k=0; k < n; k++) env_dB_block->data[k] = maxdB + db2(env[k]); //maxdb in the private section 
      
      // apply wide-dynamic range compression
      WDRC_circuit_gain(env_dB_block->data, gain_out, n, tkgn, tk, cr, bolt);
      AudioStream_F32::release(env_dB_block);
    }

    //original call to WDRC_circuit
    //void WDRC_circuit(float *x, float *y, float *pdb, int n, float tkgn, float tk, float cr, float bolt)
    //void WDRC_circuit(float *orig_signal, float *signal_out, float *env_dB, int n, float tkgn, float tk, float cr, float bolt)
    //modified to output just the gain instead of the fully processed signal
    void WDRC_circuit_gain(float *env_dB, float *gain_out, const int n, 
        const float tkgn, const float tk, const float cr, const float bolt) 
    	//tkgn = gain (dB?) at start of compression (ie, gain for linear behavior?)
    	//tk = compression start kneepoint (pre-compression, dB SPL?)
    	//cr = compression ratio
    	//bolt = broadband output limiting threshold (post-compression, dB SPL?)
    {
    	
    	//tkgain = 30; tk = 50;  bolt = 100; cr = 3;
      
      float gdb, tkgo, pblt;
      int k;
      float *pdb = env_dB; //just rename it to keep the code below unchanged (input SPL dB)
      float tk_tmp = tk;   //temporary, threshold for start of compression (input SPL dB)
      
      if ((tk_tmp + tkgn) > bolt) { //after gain, would the compression threshold be above the output-limitting threshold ("bolt")
          tk_tmp = bolt - tkgn;  //if so, lower the compression threshold to be the pre-gain value resulting in "bolt"
      }
      
	  tkgo = tkgn + tk_tmp * (1.0f - 1.0f / cr);  //intermediate calc
	  pblt = cr * (bolt - tkgo); //calc input level (dB) where we need to start limiting, no just compression
      const float cr_const = ((1.0f / cr) - 1.0f); //pre-calc a constant that we'll need later
      
      for (k = 0; k < n; k++) {  //loop over each sample
        if ((pdb[k] < tk_tmp) && (cr >= 1.0f)) {  //if below threshold and we're compressing
            gdb = tkgn;  //we're in the linear region.  Apply linear gain.
        } else if (pdb[k] > pblt) { //we're beyond the compression region into the limitting region
            gdb = bolt + ((pdb[k] - pblt) / 10.0f) - pdb[k]; //10:1 limiting!
        } else {
            gdb = cr_const * pdb[k] + tkgo;            
        }
        gain_out[k] = undb2(gdb);
        //y[k] = x[k] * undb2(gdb); //apply the gain
		
	  }
	  last_gain = gain_out[n-1];  //hold this value, in case the user asks for it later (not needed for the algorithm)
    }
    
    void setDefaultValues(void) { //set as limiter
      BTNRH_WDRC::CHA_WDRC gha = {
		5.0f, // attack time (ms)
        50.0f,     // release time (ms)
        24000.0f,  // fs, sampling rate (Hz), THIS IS IGNORED!
        115.0f,    // maxdB, maximum signal (dB SPL)...assumed SPL for full-scale input signal
        0.0f,      // tkgain, compression-start gain (dB)
        55.0f,    // tk, compression-start kneepoint (dB SPL)
        1.0f,     // cr, compression ratio  (set to 1.0 to defeat)
        100.0f     // bolt, broadband output limiting threshold (ie, the limiter. SPL. 10:1 comp ratio)
      };
      //setParams(gha.maxdB, gha.tkgain, gha.cr, gha.tk, gha.bolt); //also sets calcEnvelope
      setParams_from_CHA_WDRC(&gha);
    }
    void setParams_from_CHA_WDRC(BTNRH_WDRC::CHA_WDRC *gha) {
      setParams(gha->maxdB, gha->tkgain, gha->cr, gha->tk, gha->bolt); //also sets calcEnvelope
    }
    void setParams(float _maxdB, float _tkgain, float _cr, float _tk, float _bolt) {
      maxdB = _maxdB;
      tkgn = _tkgain;
      tk = _tk;
      cr = _cr;
      bolt = _bolt;
    }
	
	void setKneeLimiter_dBSPL(float _bolt) { bolt = _bolt; }
	void setKneeLimiter_dBFS(float _bolt_dBFS) {  //convert to dB SPL
		float bolt_dBSPL = maxdB + _bolt_dBFS;
		setKneeLimiter_dBSPL(bolt_dBSPL);
	}
	void setGain_dB(float _gain_dB) { tkgn = _gain_dB; } //gain at start of compression
	void setKneeCompressor_dBSPL(float _tk) { tk = _tk; }
	void setKneeCompressor_dBFS(float _tk_dBFS) {  // convert to dB SPL
		float tk_dBSPL = maxdB + _tk_dBFS;
		setKneeCompressor_dBSPL(tk_dBSPL);
	}
	void setCompRatio(float _cr) { cr = _cr; };
	void setMaxdB(float _maxdB) { maxdB = _maxdB; }
		
	    
    float getGain_dB(void) { return tkgn;  }	//returns the linear gain of the system
	float getCurrentGain(void) { return last_gain; }
	float getCurrentGain_dB(void) { return db2(getCurrentGain()); }
		
    //dB functions.  Feed it the envelope amplitude (not squared) and it computes 20*log10(x) or it does 10.^(x/20)
    static float undb2(const float &x)  { return expf(0.11512925464970228420089957273422f*x); } //faster:  exp(log(10.0f)*x/20);  this is exact
    static float db2(const float &x)  { return 6.020599913279623f*log2f_approx(x); } //faster: 20*log2_approx(x)/log2(10);  this is approximate

    /* ----------------------------------------------------------------------
    ** Fast approximation to the log2() function.  It uses a two step
    ** process.  First, it decomposes the floating-point number into
    ** a fractional component F and an exponent E.  The fraction component
    ** is used in a polynomial approximation and then the exponent added
    ** to the result.  A 3rd order polynomial is used and the result
    ** when computing db20() is accurate to 7.984884e-003 dB.
    ** ------------------------------------------------------------------- */
    //https://community.arm.com/tools/f/discussions/4292/cmsis-dsp-new-functionality-proposal/22621#22621
    static float log2f_approx(float X) {
      //float *C = &log2f_approx_coeff[0];
      float Y;
      float F;
      int E;
    
      // This is the approximation to log2()
      F = frexpf(fabsf(X), &E);
      //  Y = C[0]*F*F*F + C[1]*F*F + C[2]*F + C[3] + E;
      Y = 1.23149591368684f; //C[0]
      Y *= F;
      Y += -4.11852516267426f;  //C[1]
      Y *= F;
      Y += 6.02197014179219f;  //C[2]
      Y *= F;
      Y += -3.13396450166353f; //C[3]
      Y += E;
    
      return(Y);
    }

  private:
    audio_block_f32_t *inputQueueArray_f32[1]; //memory pointer for the input to this module
    float maxdB, tkgn, tk, cr, bolt;
	float last_gain = 1.0;  //what was the last gain value computed for the signal
};

#endif
