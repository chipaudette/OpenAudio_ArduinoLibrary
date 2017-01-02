/*
 * AudioMixer4
 * 
 * Created: Patrick Radius, December 2016
 * Purpose: Mix up to 4 audio channels with individual gain controls.
 * Assumes floating-point data.
 *          
 * This processes a single stream fo audio data (ie, it is mono)       
 *          
 * MIT License.  use at your own risk.
*/

#ifndef AUDIOMIXER4F32_H
#define AUDIOMIXER4F32_H

#include <arm_math.h> 
#include <AudioStream_F32.h>

class AudioMixer4_F32 : public AudioStream_F32 {
public:
    AudioMixer4_F32() : AudioStream_F32(4, inputQueueArray) {
      for (int i=0; i<4; i++) multiplier[i] = 1.0;
    }

    virtual void update(void);

    void gain(unsigned int channel, float gain) {
      if (channel >= 4 || channel < 0) return;
      multiplier[channel] = gain;
    }

  private:
    audio_block_f32_t *inputQueueArray[4];
    float multiplier[4];
};

#endif