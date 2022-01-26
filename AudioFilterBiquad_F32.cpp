/*
 * AudioFilterBiquad_F32.cpp
 *
 * Chip Audette, OpenAudio, Apr 2017
 * MIT License,  Use at your own risk.
 *
 * This filter has been running in F32 as a single stage.  This
 * would work by using multiple instantations, but compute time and
 * latency suffer.  So, Feb 2021 convert to MAX_STAGES 4 as is the I16
 * Teensy Audio library.    Bob Larkin
 *
 * See AudioFilterBiquad_F32.h for more notes.
*/
#include "AudioFilterBiquad_F32.h"

void AudioFilterBiquad_F32::update(void)  {
  audio_block_f32_t *block;

  block = AudioStream_F32::receiveWritable_f32();
  if (!block) return;  // Out of memory
  if(doBiquad)   // Filter is defined, so go to it
  arm_biquad_cascade_df1_f32(&iir_inst, block->data,
           block->data, block->length);
  // Transmit the data, filtered or unfiltered
  AudioStream_F32::transmit(block);
  AudioStream_F32::release(block);
}
