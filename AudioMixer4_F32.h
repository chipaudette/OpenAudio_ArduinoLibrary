#ifndef AUDIOMIXER4F32_H
#define AUDIOMIXER4F32_H

#include <arm_math.h> 
#include <AudioStream_F32.h>


class AudioMixer4_F32 : public AudioStream_F32 {
public:
    AudioMixer4_F32() : AudioStream_F32(4, inputQueueArray) {
      for (int i=0; i<4; i++) multiplier[i] = 0.5;
    }

    virtual void update(void);

    void gain(unsigned int channel, float gain) {
      if (channel >= 4) return;
      if (gain > 127.0f) gain = 127.0f;
      else if (gain < 0.0f) gain = 0.0f;
      multiplier[channel] = gain;
    }

  private:
    audio_block_f32_t *inputQueueArray[4];
    float multiplier[4];
};

#endif