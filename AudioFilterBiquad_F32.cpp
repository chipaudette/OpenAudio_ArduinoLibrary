
/*
 * AudioFilterBiquad_F32.cpp
 *
 * Chip Audette, OpenAudio, Apr 2017
 *
 * MIT License,  Use at your own risk.
 *
*/


#include "AudioFilterBiquad_F32.h"

void AudioFilterBiquad_F32::update(void)
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
