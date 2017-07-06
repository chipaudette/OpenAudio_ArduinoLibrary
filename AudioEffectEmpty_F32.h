/*
 * AudioEffectEmpty_F32
 * 
 * Created: Chip Audette, Feb 2017
 * Purpose: This module does nothing.  It is an empty algorithm that can one
 *			can build from to make their own algorithm
 *          
 * This processes a single stream fo audio data (ie, it is mono)       
 *          
 * MIT License.  use at your own risk.
*/

#ifndef _AudioEffectEmpty_F32_h
#define _AudioEffectEmpty_F32_h

#include <arm_math.h> //ARM DSP extensions.  for speed!
#include <AudioStream_F32.h>

class AudioEffectEmpty_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  //GUI: shortName:empty
  public:
    //constructor
    AudioEffectEmpty_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {};
	AudioEffectEmpty_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) {};

    //here's the method that does all the work
    void update(void) {
		
		//Serial.println("AudioEffectEmpty_F32: updating.");  //for debugging.
		audio_block_f32_t *block;
		block = AudioStream_F32::receiveWritable_f32();
		if (!block) return;

		//add your processing here!
		
		
		//transmit the block and be done
		AudioStream_F32::transmit(block);
		AudioStream_F32::release(block);
    }

    
  private:
    audio_block_f32_t *inputQueueArray_f32[1]; //memory pointer for the input to this module

};

#endif