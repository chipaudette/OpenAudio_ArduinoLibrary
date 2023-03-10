#include "AudioMathConstant_F32.h"

void AudioMathConstant_F32::update(void) {
  audio_block_f32_t *block;

  block = AudioStream_F32::allocate_f32();
  if (!block) return;

  for(int i=0; i<128; i++)
     block->data[i] = constant;
  AudioStream_F32::transmit(block);
  AudioStream_F32::release(block);
  }
