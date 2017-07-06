/*
   AudioEffectCompressor

   Created: Chip Audette, Dec 2016 - Jan 2017
   Purpose; Apply dynamic range compression to the audio stream.
            Assumes floating-point data.

   This processes a single stream fo audio data (ie, it is mono)

   MIT License.  use at your own risk.
*/

#ifndef _AudioEffectCompressor_F32
#define _AudioEffectCompressor_F32

#include <arm_math.h> //ARM DSP extensions.  https://www.keil.com/pack/doc/CMSIS/DSP/html/index.html
#include <AudioStream_F32.h>

class AudioEffectCompressor_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  public:
    //constructor
    AudioEffectCompressor_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {
	  setDefaultValues(AUDIO_SAMPLE_RATE);   resetStates();
    };
	
    AudioEffectCompressor_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) {
	  setDefaultValues(settings.sample_rate_Hz);   resetStates();
    };
	
	void setDefaultValues(const float sample_rate_Hz) {
      setThresh_dBFS(-20.0f);     //set the default value for the threshold for compression
      setCompressionRatio(5.0f);  //set the default copression ratio
      setAttack_sec(0.005f, sample_rate_Hz);  //default to this value
      setRelease_sec(0.200f, sample_rate_Hz); //default to this value
      setHPFilterCoeff();  enableHPFilter(true);  //enable the HP filter to remove any DC offset from the audio		
	}

    //here's the method that does all the work
    void update(void) {
      //Serial.println("AudioEffectGain_F32: updating.");  //for debugging.
      audio_block_f32_t *audio_block = AudioStream_F32::receiveWritable_f32();
      if (!audio_block) return;

      //apply a high-pass filter to get rid of the DC offset
      if (use_HP_prefilter) arm_biquad_cascade_df1_f32(&hp_filt_struct, audio_block->data, audio_block->data, audio_block->length);
      
      //apply the pre-gain...a negative gain value will disable
      if (pre_gain > 0.0f) arm_scale_f32(audio_block->data, pre_gain, audio_block->data, audio_block->length); //use ARM DSP for speed!

      //calculate the level of the audio (ie, calculate a smoothed version of the signal power)
      audio_block_f32_t *audio_level_dB_block = AudioStream_F32::allocate_f32();
      calcAudioLevel_dB(audio_block, audio_level_dB_block); //returns through audio_level_dB_block

      //compute the desired gain based on the observed audio level
      audio_block_f32_t *gain_block = AudioStream_F32::allocate_f32();
      calcGain(audio_level_dB_block, gain_block);  //returns through gain_block

      //apply the desired gain...store the processed audio back into audio_block
      arm_mult_f32(audio_block->data, gain_block->data, audio_block->data, audio_block->length);

      //transmit the block and release memory
      AudioStream_F32::transmit(audio_block);
      AudioStream_F32::release(audio_block);
      AudioStream_F32::release(gain_block);
      AudioStream_F32::release(audio_level_dB_block);
    }

    // Here's the method that estimates the level of the audio (in dB)
    // It squares the signal and low-pass filters to get a time-averaged
    // signal power.  It then 
    void calcAudioLevel_dB(audio_block_f32_t *wav_block, audio_block_f32_t *level_dB_block) { 
    	
      // calculate the instantaneous signal power (square the signal)
      audio_block_f32_t *wav_pow_block = AudioStream_F32::allocate_f32();
      arm_mult_f32(wav_block->data, wav_block->data, wav_pow_block->data, wav_block->length);

      // low-pass filter and convert to dB
      float c1 = level_lp_const, c2 = 1.0f - c1; //prepare constants
      for (int i = 0; i < wav_pow_block->length; i++) {
        // first-order low-pass filter to get a running estimate of the average power
        wav_pow_block->data[i] = c1*prev_level_lp_pow + c2*wav_pow_block->data[i];
        
        // save the state of the first-order low-pass filter
        prev_level_lp_pow = wav_pow_block->data[i]; 

        //now convert the signal power to dB (but not yet multiplied by 10.0)
        level_dB_block->data[i] = log10f_approx(wav_pow_block->data[i]);
      }

      //limit the amount that the state of the smoothing filter can go toward negative infinity
      if (prev_level_lp_pow < (1.0E-13)) prev_level_lp_pow = 1.0E-13;  //never go less than -130 dBFS 

      //scale the wav_pow_block by 10.0 to complete the conversion to dB
      arm_scale_f32(level_dB_block->data, 10.0f, level_dB_block->data, level_dB_block->length); //use ARM DSP for speed!

      //release memory and return
      AudioStream_F32::release(wav_pow_block);
      return; //output is passed through level_dB_block
    }

    //This method computes the desired gain from the compressor, given an estimate
    //of the signal level (in dB)
    void calcGain(audio_block_f32_t *audio_level_dB_block, audio_block_f32_t *gain_block) { 
    
      //first, calculate the instantaneous target gain based on the compression ratio
      audio_block_f32_t *inst_targ_gain_dB_block = AudioStream_F32::allocate_f32(); 
      calcInstantaneousTargetGain(audio_level_dB_block, inst_targ_gain_dB_block);
    
      //second, smooth in time (attack and release) by stepping through each sample
      audio_block_f32_t *gain_dB_block = AudioStream_F32::allocate_f32(); 
      calcSmoothedGain_dB(inst_targ_gain_dB_block,gain_dB_block);

      //finally, convert from dB to linear gain: gain = 10^(gain_dB/20);  (ie this takes care of the sqrt, too!)
      arm_scale_f32(gain_dB_block->data, 1.0f/20.0f, gain_dB_block->data, gain_dB_block->length);  //divide by 20 
      for (int i = 0; i < gain_dB_block->length; i++) gain_block->data[i] = pow10f(gain_dB_block->data[i]); //do the 10^(x)
      

      //release memory and return
      AudioStream_F32::release(gain_dB_block);
      AudioStream_F32::release(inst_targ_gain_dB_block);
      return;  //output is passed through gain_block
    }
      
    //Compute the instantaneous desired gain, including the compression ratio and
    //threshold for where the comrpession kicks in
    void calcInstantaneousTargetGain(audio_block_f32_t *audio_level_dB_block, audio_block_f32_t *inst_targ_gain_dB_block) {
      
      // how much are we above the compression threshold?
      audio_block_f32_t *above_thresh_dB_block = AudioStream_F32::allocate_f32(); 
      arm_offset_f32(audio_level_dB_block->data,  //CMSIS DSP for "add a constant value to all elements"
        -thresh_dBFS,                         //this is the value to be added
        above_thresh_dB_block->data,          //this is the output
        audio_level_dB_block->length);  

      // scale by the compression ratio...this is what the output level should be (this is our target level)
      arm_scale_f32(above_thresh_dB_block->data,    //CMSIS DSP for "multiply all elements by a constant value"
           1.0f / comp_ratio,                       //this is the value to be multiplied 
           inst_targ_gain_dB_block->data,           //this is the output
           above_thresh_dB_block->length); 

      // compute the instantaneous gain...which is the difference between the target level and the original level
      arm_sub_f32(inst_targ_gain_dB_block->data,  //CMSIS DSP for "subtract two vectors element-by-element"
           above_thresh_dB_block->data,           //this is the vector to be subtracted
           inst_targ_gain_dB_block->data,         //this is the output
           inst_targ_gain_dB_block->length);

      // limit the target gain to attenuation only (this part of the compressor should not make things louder!)
      for (int i=0; i < inst_targ_gain_dB_block->length; i++) {
        if (inst_targ_gain_dB_block->data[i] > 0.0f) inst_targ_gain_dB_block->data[i] = 0.0f;
      }

      // release memory before returning
      AudioStream_F32::release(above_thresh_dB_block);
      return;  //output is passed through inst_targ_gain_dB_block
    }

    //this method applies the "attack" and "release" constants to smooth the
    //target gain level through time.
    void calcSmoothedGain_dB(audio_block_f32_t *inst_targ_gain_dB_block, audio_block_f32_t *gain_dB_block) {
      float32_t gain_dB;
      float32_t one_minus_attack_const = 1.0f - attack_const;
      float32_t one_minus_release_const = 1.0f - release_const;
      for (int i = 0; i < inst_targ_gain_dB_block->length; i++) {
        gain_dB = inst_targ_gain_dB_block->data[i];

        //smooth the gain using the attack or release constants
        if (gain_dB < prev_gain_dB) {  //are we in the attack phase?
          gain_dB_block->data[i] = attack_const*prev_gain_dB + one_minus_attack_const*gain_dB;
        } else {   //or, we're in the release phase
          gain_dB_block->data[i] = release_const*prev_gain_dB + one_minus_release_const*gain_dB;
        }

        //save value for the next time through this loop
        prev_gain_dB = gain_dB_block->data[i];
      }

      //return
      return;  //the output here is gain_block
    }


    //methods to set parameters of this module
    void resetStates(void) {
      prev_level_lp_pow = 1.0f;
      prev_gain_dB = 0.0f;
      
      //initialize the HP filter.  (This also resets the filter states,)
      arm_biquad_cascade_df1_init_f32(&hp_filt_struct, hp_nstages, hp_coeff, hp_state);
    }
    void setPreGain(float g) {  pre_gain = g;  }
    void setPreGain_dB(float gain_dB) { setPreGain(pow(10.0, gain_dB / 20.0));  }
    void setCompressionRatio(float cr) {
      comp_ratio = max(0.001, cr); //limit to positive values
      updateThresholdAndCompRatioConstants();
    }
    void setAttack_sec(float a, float fs_Hz) {
      attack_sec = a;
      attack_const = expf(-1.0f / (attack_sec * fs_Hz)); //expf() is much faster than exp()

      //also update the time constant for the envelope extraction
      setLevelTimeConst_sec(min(attack_sec,release_sec) / 5.0, fs_Hz);  //make the level time-constant one-fifth the gain time constants
    } 
    void setRelease_sec(float r, float fs_Hz) {
      release_sec = r;
      release_const = expf(-1.0f / (release_sec * fs_Hz)); //expf() is much faster than exp()

      //also update the time constant for the envelope extraction
      setLevelTimeConst_sec(min(attack_sec,release_sec) / 5.0, fs_Hz);  //make the level time-constant one-fifth the gain time constants
    }
    void setLevelTimeConst_sec(float t_sec, float fs_Hz) {
      const float min_t_sec = 0.002f;  //this is the minimum allowed value
      level_lp_sec = max(min_t_sec,t_sec);
      level_lp_const = expf(-1.0f / (level_lp_sec * fs_Hz)); //expf() is much faster than exp()
    }
    void setThresh_dBFS(float val) { 
      thresh_dBFS = val;
      setThreshPow(pow(10.0, thresh_dBFS / 10.0));
    }
    void enableHPFilter(boolean flag) { use_HP_prefilter = flag; };

    //methods to return information about this module
    float32_t getPreGain_dB(void) { return 20.0 * log10f_approx(pre_gain);  }
    float32_t getAttack_sec(void) {  return attack_sec; }
    float32_t getRelease_sec(void) {  return release_sec; }
    float32_t getLevelTimeConst_sec(void) { return level_lp_sec; }
    float32_t getThresh_dBFS(void) { return thresh_dBFS; }
    float32_t getCompressionRatio(void) { return comp_ratio; }
    float32_t getCurrentLevel_dBFS(void) { return 10.0* log10f_approx(prev_level_lp_pow); }
    float32_t getCurrentGain_dB(void) { return prev_gain_dB; }

    void setHPFilterCoeff_N2IIR_Matlab(float32_t b[], float32_t a[]){
      //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html#ga8e73b69a788e681a61bccc8959d823c5
      //Use matlab to compute the coeff for HP at 20Hz: [b,a]=butter(2,20/(44100/2),'high'); %assumes fs_Hz = 44100
      hp_coeff[0] = b[0];   hp_coeff[1] = b[1];  hp_coeff[2] = b[2]; //here are the matlab "b" coefficients
      hp_coeff[3] = -a[1];  hp_coeff[4] = -a[2];  //the DSP needs the "a" terms to have opposite sign vs Matlab    	
    }
    
  private:
    //state-related variables
    audio_block_f32_t *inputQueueArray_f32[1]; //memory pointer for the input to this module
    float32_t prev_level_lp_pow = 1.0;
    float32_t prev_gain_dB = 0.0; //last gain^2 used

    //HP filter state-related variables
    arm_biquad_casd_df1_inst_f32 hp_filt_struct;
    static const uint8_t hp_nstages = 1;
    float32_t hp_coeff[5 * hp_nstages] = {1.0, 0.0, 0.0, 0.0, 0.0}; //no filtering. actual filter coeff set later
    float32_t hp_state[4 * hp_nstages];
    void setHPFilterCoeff(void) {
      //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html#ga8e73b69a788e681a61bccc8959d823c5
      //Use matlab to compute the coeff for HP at 20Hz: [b,a]=butter(2,20/(44100/2),'high'); %assumes fs_Hz = 44100
      float32_t b[] = {9.979871156751189e-01,    -1.995974231350238e+00, 9.979871156751189e-01};  //from Matlab
      float32_t a[] = { 1.000000000000000e+00,    -1.995970179642828e+00,    9.959782830576472e-01};  //from Matlab
      setHPFilterCoeff_N2IIR_Matlab(b, a);
      //hp_coeff[0] = b[0];   hp_coeff[1] = b[1];  hp_coeff[2] = b[2]; //here are the matlab "b" coefficients
      //hp_coeff[3] = -a[1];  hp_coeff[4] = -a[2];  //the DSP needs the "a" terms to have opposite sign vs Matlab
    }


    //private parameters related to gain calculation
    float32_t attack_const, release_const, level_lp_const; //used in calcGain().  set by setAttack_sec() and setRelease_sec();
    float32_t comp_ratio_const, thresh_pow_FS_wCR;  //used in calcGain();  set in updateThresholdAndCompRatioConstants()
    void updateThresholdAndCompRatioConstants(void) {
      comp_ratio_const = 1.0f-(1.0f / comp_ratio);
      thresh_pow_FS_wCR = powf(thresh_pow_FS, comp_ratio_const);    
    }

    //settings
    float32_t attack_sec, release_sec, level_lp_sec; 
    float32_t thresh_dBFS = 0.0;  //threshold for compression, relative to digital full scale
    float32_t thresh_pow_FS = 1.0f;  //same as above, but not in dB
    void setThreshPow(float t_pow) { 
      thresh_pow_FS = t_pow;
      updateThresholdAndCompRatioConstants();
    }
    float32_t comp_ratio = 1.0;  //compression ratio
    float32_t pre_gain = -1.0;  //gain to apply before the compression.  negative value disables
    boolean use_HP_prefilter;
    
    
    // Accelerate the powf(10.0,x) function
    static float32_t pow10f(float x) {
      //return powf(10.0f,x)   //standard, but slower
      return expf(2.302585092994f*x);  //faster:  exp(log(10.0f)*x)
    }

    // Accelerate the log10f(x)  function?
    static float32_t log10f_approx(float x) {
      //return log10f(x);   //standard, but slower
      return log2f_approx(x)*0.3010299956639812f; //faster:  log2(x)/log2(10)
    }
    
    /* ----------------------------------------------------------------------
    ** Fast approximation to the log2() function.  It uses a two step
    ** process.  First, it decomposes the floating-point number into
    ** a fractional component F and an exponent E.  The fraction component
    ** is used in a polynomial approximation and then the exponent added
    ** to the result.  A 3rd order polynomial is used and the result
    ** when computing db20() is accurate to 7.984884e-003 dB.
    ** ------------------------------------------------------------------- */
    //https://community.arm.com/tools/f/discussions/4292/cmsis-dsp-new-functionality-proposal/22621#22621
    //float log2f_approx_coeff[4] = {1.23149591368684f, -4.11852516267426f, 6.02197014179219f, -3.13396450166353f};
    static float log2f_approx(float X) {
      //float *C = &log2f_approx_coeff[0];
      float Y;
      float F;
      int E;
    
      // This is the approximation to log2()
      F = frexpf(fabsf(X), &E);
      //  Y = C[0]*F*F*F + C[1]*F*F + C[2]*F + C[3] + E;
      //Y = *C++;
      Y = 1.23149591368684f;
      Y *= F;
      //Y += (*C++);
      Y += -4.11852516267426f;
      Y *= F;
      //Y += (*C++);
      Y += 6.02197014179219f;
      Y *= F;
      //Y += (*C++);
      Y += -3.13396450166353f;
      Y += E;
    
      return(Y);
    }
    
    
};

#endif

