/* 
*	AudioSynthNoiseWhite_F32
*
*	Created: Chip Audette (OpenAudio), Feb 2017
*       Extended from on Teensy Audio Library
*
*	License: MIT License.  Use at your own risk.
*/

#ifndef synth_pinknoise_f32_h_
#define synth_pinknoise_f32_h_
#include "Arduino.h"
#include "AudioStream_F32.h"
#include <Audio.h>
#include "utility/dspinst.h"

class AudioSynthNoisePink_F32 : public AudioStream_F32
{
//GUI: inputs:0, outputs:1 //this line used for automatic generation of GUI node
//GUI: shortName:pinknoise  //this line used for automatic generation of GUI node
public:
	AudioSynthNoisePink_F32() : AudioStream_F32(0, NULL) {
		output_queue.begin();
		
		patchCord100 = new AudioConnection(noise, 0, i16_to_f32, 0);  //noise is an Int16 audio object.  So, convert it!
    	patchCord101 = new AudioConnection_F32(i16_to_f32, 0, output_queue, 0);
	}
	
	//define audio processing stack right here.
    AudioSynthNoisePink  	noise;
    AudioConvert_I16toF32   i16_to_f32; 
    AudioRecordQueue_F32    output_queue;    
    AudioConnection     *patchCord100;
    AudioConnection_F32 *patchCord101;
	
    void update(void) {
      output_queue.clear();
    	
      //manually update audio blocks in the desired order
      noise.update();  //the output should be routed directly via the AudioConnection
      i16_to_f32.update();  // output is routed via the AudioConnection
      output_queue.update();
      
      //get the output
      audio_block_f32_t  *block = output_queue.getAudioBlock();
      if (block == NULL) return;

      //transmit the block, and release memory
      AudioStream_F32::transmit(block);
      output_queue.freeAudioBlock();
    }
    void amplitude(float n) {
    	noise.amplitude(n);
    }
    
private:    

};

#endif