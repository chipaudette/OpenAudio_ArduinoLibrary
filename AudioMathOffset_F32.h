/*
 * AudioMathOffset_F32
 * 
 * Created: Chip Audette, Open Audio, June 2018
 * Purpose: Add a constant DC value to all audio samples
 * Assumes floating-point data.
 *          
 * This processes a single stream fo audio data (ie, it is mono)       
 *          
 * MIT License.  use at your own risk.
*/
#ifndef AudioMathOffset_F32_H
#define AudioMathOffset_F32_H

#include <arm_math.h>
#include "AudioStream_F32.h"

class AudioMathOffset_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  public:
    AudioMathOffset_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {};
	AudioMathOffset_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) {};
	
    void update(void);
    float setOffset(float _offset) { return offset = _offset;}
	float getOffset(void) { return offset; }
  private:
    audio_block_f32_t *inputQueueArray_f32[1];
	float32_t offset = 0.0f;
};

#endif
