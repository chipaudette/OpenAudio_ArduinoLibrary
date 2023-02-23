/*
 * AudioMathAdd_F32
 * 
 * Created: Chip Audette, Open Audio, July 2018
 * Purpose: Add together two channels (vectors) of audio data on a point-by-point basis
 *     (like AudioMathMutiply, but addition).  Assumes floating-point data.
 *          
 * This processes a single stream fo audio data (ie, it is mono)       
 *          
 * MIT License.  use at your own risk.
*/
#ifndef _AudioMathAdd_F32_H
#define _AudioMathAdd_F32_H

#include <arm_math.h>
#include "AudioStream_F32.h"

class AudioMathAdd_F32 : public AudioStream_F32
{
  //GUI: inputs:2, outputs:1  //this line used for automatic generation of GUI node
  public:
    AudioMathAdd_F32(void) : AudioStream_F32(2, inputQueueArray_f32) {};
	AudioMathAdd_F32(const AudioSettings_F32 &settings) : AudioStream_F32(2, inputQueueArray_f32) {};
	
    void update(void);
    
  private:
    audio_block_f32_t *inputQueueArray_f32[2];
};

#endif
