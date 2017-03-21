/*
 * AudioFilterIIR_F32
 * 
 * Created: Chip Audette (OpenAudio) Feb 2017
 *
 * License: MIT License.  Use at your own risk.
 * 
 */

#ifndef _filter_iir_f32
#define _filter_iir_f32

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"

// Indicates that the code should just pass through the audio
// without any filtering (as opposed to doing nothing at all)
#define IIR_F32_PASSTHRU ((const float32_t *) 1)

#define IIR_MAX_STAGES 1  //meaningless right now

class AudioFilterIIR_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  //GUI: shortName:IIR
  public:
    AudioFilterIIR_F32(void): AudioStream_F32(1,inputQueueArray), coeff_p(FIR_F32_PASSTHRU) {
    }
    void begin(const float32_t *cp, int n_stages) {
      coeff_p = cp;
      // Initialize FIR instance (ARM DSP Math Library)
      if (coeff_p && (coeff_p != IIR_F32_PASSTHRU) && n_stages <= IIR_MAX_STAGES) {
        //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html
        arm_biquad_cascade_df1_init_f32(&iir_inst, n_stages, (float32_t *)coeff_p,  &StateF32[0]);
      }
    }
    void end(void) {
      coeff_p = NULL;
    }
    
    void setBlockDC(void) {
      //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html#ga8e73b69a788e681a61bccc8959d823c5
      //Use matlab to compute the coeff for HP at 40Hz: [b,a]=butter(2,40/(44100/2),'high'); %assumes fs_Hz = 44100
      float32_t b[] = {8.173653471988667e-01,  -1.634730694397733e+00,   8.173653471988667e-01};  //from Matlab
      float32_t a[] = { 1.000000000000000e+00,   -1.601092394183619e+00,  6.683689946118476e-01};  //from Matlab
      setFilterCoeff_Matlab(b, a);
    }
    
    void setFilterCoeff_Matlab(float32_t b[], float32_t a[]) { //one stage of N=2 IIR
      //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html#ga8e73b69a788e681a61bccc8959d823c5
      //Use matlab to compute the coeff, such as: [b,a]=butter(2,20/(44100/2),'high'); %assumes fs_Hz = 44100
      hp_coeff[0] = b[0];   hp_coeff[1] = b[1];  hp_coeff[2] = b[2]; //here are the matlab "b" coefficients
      hp_coeff[3] = -a[1];  hp_coeff[4] = -a[2];  //the DSP needs the "a" terms to have opposite sign vs Matlab 
      uint8_t n_stages = 1;
      arm_biquad_cascade_df1_init_f32(&iir_inst, n_stages, hp_coeff,  &StateF32[0]);     
    }
    
    virtual void update(void);
   
  private:
    audio_block_f32_t *inputQueueArray[1];
    float32_t hp_coeff[5 * 1] = {1.0, 0.0, 0.0, 0.0, 0.0}; //no filtering. actual filter coeff set later
  
    // pointer to current coefficients or NULL or FIR_PASSTHRU
    const float32_t *coeff_p;
  
    // ARM DSP Math library filter instance
    arm_biquad_casd_df1_inst_f32 iir_inst;
    float32_t StateF32[4*IIR_MAX_STAGES];
};


void AudioFilterIIR_F32::update(void)
{
  audio_block_f32_t *block;

  block = AudioStream_F32::receiveWritable_f32();
  if (!block) return;

  // If there's no coefficient table, give up.  
  if (coeff_p == NULL) {
    AudioStream_F32::release(block);
    return;
  }

  // do passthru
  if (coeff_p == IIR_F32_PASSTHRU) {
    // Just passthrough
    AudioStream_F32::transmit(block);
    AudioStream_F32::release(block);
    return;
  }

  // do IIR
  arm_biquad_cascade_df1_f32(&iir_inst, block->data, block->data, block->length);
  
  //transmit the data
  AudioStream_F32::transmit(block); // send the IIR output
  AudioStream_F32::release(block);
}

#endif


