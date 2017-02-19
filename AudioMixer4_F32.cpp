#include "AudioMixer4_F32.h"

void AudioMixer4_F32::update(void) {
  audio_block_f32_t *in, *out=NULL;

  out = receiveWritable_f32(0);
  if (!out) return;

  arm_scale_f32(out->data, multiplier[0], out->data, out->length);

  for (int channel=1; channel < 4; channel++) {
    in = receiveReadOnly_f32(channel);
    if (!in) {
      continue;
    }

    audio_block_f32_t *tmp = allocate_f32();

    arm_scale_f32(in->data, multiplier[channel], tmp->data, tmp->length);
    arm_add_f32(out->data, tmp->data, out->data, tmp->length);

    release(tmp);
    release(in);
  }

  if (out) {
    transmit(out);
    release(out);
  }
}
