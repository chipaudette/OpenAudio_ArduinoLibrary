/*
   AudioEffectCompressor

   Created: Chip Audette, December 2016
   Purpose; Apply dynamic range compression to the audio stream.
            Assumes floating-point data.

   This processes a single stream fo audio data (ie, it is mono)

   MIT License.  use at your own risk.
*/

#include <arm_math.h> //ARM DSP extensions.  https://www.keil.com/pack/doc/CMSIS/DSP/html/index.html
#include <AudioStream_F32.h>

class AudioEffectCompressor_F32 : public AudioStream_F32
{
  public:
    //constructor
    AudioEffectCompressor_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {
      setThresh_dBFS(-20.0f);     //default to this threshold
      setAttack_sec(0.005f, AUDIO_SAMPLE_RATE);  //default to this value
      setRelease_sec(0.200f, AUDIO_SAMPLE_RATE); //default to this value
      setCompressionRatio(5.0f);  //default to this value
      setThresh_dBFS(-20.0f);  //default to this value
      setHPFilterCoeff();
      resetStates();
    };

    //here's the method that does all the work
    void update(void) {
      //Serial.println("AudioEffectGain_F32: updating.");  //for debugging.
      audio_block_f32_t *audio_block;
      audio_block = AudioStream_F32::receiveWritable_f32();
      if (!audio_block) return;

      //apply a high-pass filter to get rid of the DC offset
      if (use_HP_prefilter) arm_biquad_cascade_df1_f32(&hp_filt_struct, audio_block->data, audio_block->data, audio_block->length);

      //apply the pre-gain...a negative gain value will disable
      if (pre_gain > 0.0f) arm_scale_f32(audio_block->data, pre_gain, audio_block->data, audio_block->length); //use ARM DSP for speed!

      //compute the desired gain
      audio_block_f32_t *gain_block = AudioStream_F32::allocate_f32();
      calcGain(audio_block, gain_block); //returns through gain_block

      //apply the gain...store it back into audio_block
      arm_mult_f32(audio_block->data, gain_block->data, audio_block->data, audio_block->length);

      ///transmit the block and release memory
      AudioStream_F32::transmit(audio_block);
      AudioStream_F32::release(audio_block);
      AudioStream_F32::release(gain_block);
    }

    void calcGain(audio_block_f32_t *wav_block, audio_block_f32_t *gain_block) {      

      //calculate the signal power...ie, square the signal:   wav_pow = wav.^2
      audio_block_f32_t *wav_pow_block = AudioStream_F32::allocate_f32();
      arm_mult_f32(wav_block->data, wav_block->data, wav_pow_block->data, wav_block->length);

      //loop over each sample
      float32_t gain_pow;
      for (int i = 0; i < wav_pow_block->length; i++) {

        //compute target gain (well, we're actualy calculating gain^2) assuming we want to copress
        gain_pow = thresh_pow_FS_wCR / powf(wav_pow_block->data[i], comp_ratio_const);

        //if our signal level is below the threshold, don't compress (set target gain to 0dB, which is 1.0)
        if (wav_pow_block->data[i] < thresh_pow_FS) gain_pow = 1.0f;

        //are we in the attack mode or release mode?
        float32_t c = attack_const; //at first, assume that we're in the attack phase
        if (gain_pow > prev_gain_pow) c = release_const;  //here, we decide if we're really in the release phase

        //smooth the gain using the attack or release constants
        gain_pow = c*prev_gain_pow + (1.0f-c)*gain_pow;

        //take he sqrt of gain^2 so that we simply get the gain
        //arm_sqrt_f32(gain_pow, &(gain_block->data[i])); //should use the DSP acceleration, if the right CMSIS library is used
        //gain_block->data[i] = __builtin_sqrtf(gain_pow); //seems to give the same speed as the arm_sqrt_f32
        gain_block->data[i] = sqrtf(gain_pow);  //also give the same speed and is more portable

        //save value for the next time through this loop
        prev_gain_pow = gain_pow;
      }

      //free up the memory and return
      release(wav_pow_block);
      return;  //the output here is gain_block
    }

    //methods to set parameters of this module
    void resetStates(void) {
      prev_gain_pow = 1.0f;

      //initialize the HP filter (it also resets the filter states)
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
    } 
    void setRelease_sec(float r, float fs_Hz) {
      release_sec = r;
      release_const = expf(-1.0f / (release_sec * fs_Hz)); //expf() is much faster than exp()
    } 
    void setThresh_dBFS(float thresh_dBFS) { setThreshPow(pow(10.0, thresh_dBFS / 10.0)); }
    void setThreshPow(float t_pow) { 
      thresh_pow_FS = t_pow;
      updateThresholdAndCompRatioConstants();
    }
    void enableHPFilter(boolean flag) { use_HP_prefilter = flag; };

    //methods to return information about this module
    float32_t getPreGain_dB(void) { return 20.0 * log10(pre_gain);  }
    float32_t getAttack_sec(void) {  return attack_sec; }
    float32_t getRelease_sec(void) {  return release_sec; }
    float32_t getThresh_dBFS(void) { return 10.0 * log10(thresh_pow_FS); }
    float32_t getCompressionRatio(void) { return comp_ratio; }
    float32_t getCurrentGain_dB(void) { return 10.0 * log10(prev_gain_pow); }

  private:
    //state-related variables
    audio_block_f32_t *inputQueueArray_f32[1]; //memory pointer for the input to this module
    float32_t prev_gain_pow = 1.0; //last gain^2 used

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
      hp_coeff[0] = b[0];   hp_coeff[1] = b[1];  hp_coeff[2] = b[2]; //here are the matlab "b" coefficients
      hp_coeff[3] = -a[1];  hp_coeff[4] = -a[2];  //the DSP needs the "a" terms to have opposite sign vs Matlab
    }

    //private parameters related to gain calculation
    float32_t attack_const, release_const; //used in calcGain().  set by setAttack_sec() and setRelease_sec();
    float32_t comp_ratio_const, thresh_pow_FS_wCR;  //used in calcGain();  set in updateThresholdAndCompRatioConstants()
    void updateThresholdAndCompRatioConstants(void) {
      comp_ratio_const = 1.0f-(1.0f / comp_ratio);
      thresh_pow_FS_wCR = powf(thresh_pow_FS, comp_ratio_const);  //powf() is much faster than pow()      
    }

    //settings
    float32_t attack_sec, release_sec; 
    float32_t thresh_pow_FS = 1.0f;  //threshold for compression, relative to digital full scale
    float32_t comp_ratio = 1.0;  //compression ratio
    float32_t pre_gain = -1.0;  //gain to apply before the compression.  negative value disables
    boolean use_HP_prefilter = false;
    
};


