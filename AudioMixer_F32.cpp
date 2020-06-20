// Fix 1 to n problem Bob Larkin June 2020
// Need to convert to TYmpan routine??

#include "AudioMixer_F32.h"

void AudioMixer4_F32::update(void) {
  audio_block_f32_t *in, *out=NULL;

  out = receiveWritable_f32(0);
  if (!out) return;

  arm_scale_f32(out->data, multiplier[0], out->data, out->length);

  for (int channel=0; channel < 4; channel++) {  // Was 1 to 3  RSL June 2020
    in = receiveReadOnly_f32(channel);
    if (!in) {
      continue;
    }

    audio_block_f32_t *tmp = allocate_f32();

    arm_scale_f32(in->data, multiplier[channel], tmp->data, tmp->length);
    arm_add_f32(out->data, tmp->data, out->data, tmp->length);

    AudioStream_F32::release(tmp);
    AudioStream_F32::release(in);
  }

  if (out) {
    AudioStream_F32::transmit(out);
    AudioStream_F32::release(out);
  }
}

void AudioMixer8_F32::update(void) {
  audio_block_f32_t *in, *out=NULL;

  out = receiveWritable_f32(0);  //try to get the first input channel
  if (!out) return;  //if it's not there, return immediately

  arm_scale_f32(out->data, multiplier[0], out->data, out->length); //scale the first input channel

  //load and process the rest of the channels
  for (int channel=0; channel < 8; channel++) {   // Was 1 to 7  RSL June 2020
    in = receiveReadOnly_f32(channel);
    if (!in) {
      continue;
    }

    audio_block_f32_t *tmp = allocate_f32();

    arm_scale_f32(in->data, multiplier[channel], tmp->data, tmp->length);
    arm_add_f32(out->data, tmp->data, out->data, tmp->length);

    AudioStream_F32::release(tmp);
    AudioStream_F32::release(in);
  }

  if (out) {
    AudioStream_F32::transmit(out);
    AudioStream_F32::release(out);
  }
}
