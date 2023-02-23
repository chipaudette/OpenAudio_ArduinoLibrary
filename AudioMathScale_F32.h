/*
 * AudioMathScale_F32
 * 
 * Created: Chip Audette, Open Audio, June 2018
 * Purpose: Multiply all audio samples by a single value (not vector)
 * Assumes floating-point data.
 *          
 * This processes a single stream fo audio data (ie, it is mono)       
 *          
 * MIT License.  use at your own risk.
*/
#ifndef AudioMathScale_F32_H
#define AudioMathScale_F32_H

#include <arm_math.h>
#include "AudioStream_F32.h"

class AudioMathScale_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  public:
    AudioMathScale_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {};
	AudioMathScale_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) {};
	
    void update(void);
    float setScale(float _scale) { return scale = _scale;}
	float getScale(void) { return scale; }
  private:
    audio_block_f32_t *inputQueueArray_f32[1];
	float32_t scale = 0.0f;
};

#endif
