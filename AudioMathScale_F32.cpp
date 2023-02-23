#include "AudioMathScale_F32.h"

void AudioMathScale_F32::update(void) {
  audio_block_f32_t *block;

  block = AudioStream_F32::receiveWritable_f32(0);
  if (!block) return;
  
  //use the ARM-optimized routine to add the offset
  arm_scale_f32(block->data, scale, block->data, block->length);

  AudioStream_F32::transmit(block);
  AudioStream_F32::release(block);
}
