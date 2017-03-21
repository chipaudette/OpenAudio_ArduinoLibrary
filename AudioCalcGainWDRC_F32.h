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

typedef struct {
    float attack;               // attack time (ms), unused in this class
    float release;              // release time (ms), unused in this class
    float fs;                   // sampling rate (Hz), set through other means in this class
    float maxdB;                // maximum signal (dB SPL)...I think this is the SPL corresponding to signal with rms of 1.0
    float tkgain;               // compression-start gain
    float tk;                   // compression-start kneepoint
    float cr;                   // compression ratio
    float bolt;                 // broadband output limiting threshold
} CHA_WDRC;


class AudioCalcGainWDRC_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  //GUI: shortName:calc_WDRCGain
  public:
    //default constructor
    AudioCalcGainWDRC_F32(void) : AudioStream_F32(1, inputQueueArray_f32) { setDefaultValues(); };

    //here's the method that does all the work
    void update(void) {
      
      //get the input audio data block
      audio_block_f32_t *in_block = AudioStream_F32::receiveReadOnly_f32(); // must be the envelope!
      if (!in_block) return;
  
      //prepare an output data block
      audio_block_f32_t *out_block = AudioStream_F32::allocate_f32();
      if (!out_block) return;
      
      // //////////////////////add your processing here!
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
  
      //convert to dB
      for (int k=0; k < n; k++) env_dB_block->data[k] = maxdB + db2(env[k]); //maxdb in the private section 
      
      // apply wide-dynamic range compression
      WDRC_circuit_gain(env_dB_block->data, gain_out, n, tkgn, tk, cr, bolt);
      AudioStream_F32::release(env_dB_block);
    }

    //original call to WDRC_circuit
    //void WDRC_circuit(float *x, float *y, float *pdb, int n, float tkgn, float tk, float cr, float bolt)
    //void WDRC_circuit(float *orig_signal, float *signal_out, float *env_dB, int n, float tkgn, float tk, float cr, float bolt)
    //modified to output the gain instead of the fully processed signal
    void WDRC_circuit_gain(float *env_dB, float *gain_out, const int n, 
        const float tkgn, const float tk, const float cr, const float bolt) {
      
      float gdb, tkgo, pblt;
      int k;
      float *pdb = env_dB; //just rename it to keep the code below unchanged
      float tk_tmp = tk;
      
      if ((tk_tmp + tkgn) > bolt) {
          tk_tmp = bolt - tkgn;
      }
      tkgo = tkgn + tk_tmp * (1.0f - 1.0f / cr);
      pblt = cr * (bolt - tkgo);
      const float cr_const = ((1.0f / cr) - 1.0f);
      for (k = 0; k < n; k++) {
        if ((pdb[k] < tk_tmp) && (cr >= 1.0f)) {
            gdb = tkgn;
        } else if (pdb[k] > pblt) {
            gdb = bolt + ((pdb[k] - pblt) / 10.0f) - pdb[k];
        } else {
            gdb = cr_const * pdb[k] + tkgo;
        }
        gain_out[k] = undb2(gdb);
        //y[k] = x[k] * undb2(gdb); //apply the gain
      }
    }
    
    void setDefaultValues(void) {
      CHA_WDRC gha = {1.0f, // attack time (ms), IGNORED HERE
        50.0f,     // release time (ms), IGNORED HERE
        24000.0f,  // fs, sampling rate (Hz), IGNORED HERE
        119.0f,    // maxdB, maximum signal (dB SPL)
        0.0f,      // tkgain, compression-start gain
        105.0f,    // tk, compression-start kneepoint
        10.0f,     // cr, compression ratio
        105.0f     // bolt, broadband output limiting threshold
      };
      //setParams(gha.maxdB, gha.tkgain, gha.cr, gha.tk, gha.bolt); //also sets calcEnvelope
      setParams_from_CHA_WDRC(&gha);
    }
    void setParams_from_CHA_WDRC(CHA_WDRC *gha) {
      setParams(gha->maxdB, gha->tkgain, gha->cr, gha->tk, gha->bolt); //also sets calcEnvelope
    }
    void setParams(float _maxdB, float _tkgain, float _cr, float _tk, float _bolt) {
      maxdB = _maxdB;
      tkgn = _tkgain;
      tk = _tk;
      cr = _cr;
      bolt = _bolt;
    }

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
};

#endif
