/*
 * AudioMathConstant_F32
 *
 * Created: Bob Larkin, March 2023
 * Purpose: Outputs a block of floating point numbers, all the same.
 * Default value is 0.0f
 *
 * This outputs a single, constant stream of audio data (ie, it is mono)
 *
 * MIT License.  use at your own risk.
*/
#ifndef _AudioMathConstant_F32_H
#define _AudioMathConstant_F32_H

#include "AudioStream_F32.h"

class AudioMathConstant_F32 : public AudioStream_F32
{
  //GUI: inputs:0, outputs:1  //this line used for automatic generation of GUI node
  public:
    AudioMathConstant_F32(void) : AudioStream_F32(0, NULL) {};
	AudioMathConstant_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL) {
		sample_rate_Hz = settings.sample_rate_Hz;
        block_size = settings.audio_block_samples;
        };

    void setConstant(float32_t _constant) { constant = _constant; }

    virtual void update(void);

  private:
    float sample_rate_Hz = AUDIO_SAMPLE_RATE;
    uint16_t block_size = AUDIO_BLOCK_SAMPLES;
    float32_t constant = 0.0f;
};
#endif
