#include "AudioMathAdd_F32.h"

void AudioMathAdd_F32::update(void) {
  audio_block_f32_t *block, *in;

  block = AudioStream_F32::receiveWritable_f32(0);
  if (!block) return;

  in = AudioStream_F32::receiveReadOnly_f32(1);
  if (!in) {
    AudioStream_F32::release(block);
    return;
  }

  arm_add_f32(block->data, in->data, block->data, block->length);
  AudioStream_F32::release(in);

  AudioStream_F32::transmit(block);
  AudioStream_F32::release(block);
}
