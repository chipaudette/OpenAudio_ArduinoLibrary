#include "AudioMultiply_F32.h"

void AudioMultiply_F32::update(void) {
  audio_block_f32_t *block, *in;

  block = AudioStream_F32::receiveWritable_f32(0);
  if (!block) return;

  in = AudioStream_F32::receiveReadOnly_f32(1);
  if (!in) {
    release(block);
    return;
  }

  arm_mult_f32(block->data, in->data, block->data, AUDIO_BLOCK_SAMPLES);
  release(in);

  transmit(block);
  release(block);
}
