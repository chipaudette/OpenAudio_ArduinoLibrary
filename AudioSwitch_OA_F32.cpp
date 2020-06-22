/*
 * AudioSwitch_F32.cpp  see AudioSwitch_F32.h for notes
 * Created: Chip Audette, OpenAudio, April 2019
 * MIT License.  use at your own risk.
*/
#include "AudioSwitch_OA_F32.h"

void AudioSwitch4_OA_F32::update(void) {
  audio_block_f32_t *out=NULL;
  
  out = receiveReadOnly_f32(0);
  if (!out) return;
  
  AudioStream_F32::transmit(out,outputChannel); //just output to the one channel
  AudioStream_F32::release(out);
}

void AudioSwitch8_OA_F32::update(void) {
  audio_block_f32_t *out=NULL;
  
  out = receiveReadOnly_f32(0);
  if (!out) return;
  
  AudioStream_F32::transmit(out,outputChannel);
  AudioStream_F32::release(out);
}
