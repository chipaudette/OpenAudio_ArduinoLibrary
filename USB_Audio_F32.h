/* 
*	USB_Audio_F32
*
*	Created: Chip Audette (OpenAudio), Mar 2017
*       Float32 wrapper for the Audio USB classes from the Teensy Audio Library
*
*	License: MIT License.  Use at your own risk.
*/

#ifndef usb_audio_f32_h_
#define usb_audio_f32_h_
//#include "Arduino.h"
#include <AudioStream_F32.h>
#include <Audio.h>

class AudioInputUSB_F32 : public AudioStream_F32
{
//GUI: inputs:0, outputs:2 //this line used for automatic generation of GUI node
//GUI: shortName:usbAudioIn  //this line used for automatic generation of GUI node
public:
	AudioInputUSB_F32() : AudioStream_F32(0, NULL) {
		//i16_to_f32.disconnectFromUpdateAll(); //requires modification to AudioStream.h
		//output_queue.disconnectFromUpdateAll(); //requires modification to AudioStream.h
		
		makeConnections();
	}
	AudioInputUSB_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL) {
		//i16_to_f32.disconnectFromUpdateAll(); //requires modification to AudioStream.h
		//output_queue.disconnectFromUpdateAll(); //requires modification to AudioStream.h
		
		makeConnections();
	}
	
	void makeConnections(void) {
		//make the audio connections
		patchCord100_L = new AudioConnection(usb_in, 0, i16_to_f32_L, 0);  //usb_in is an Int16 audio object.  So, convert it!
    	patchCord100_R = new AudioConnection(usb_in, 1, i16_to_f32_R, 0);  //usb_in is an Int16 audio object.  So, convert it!
    	patchCord101_L = new AudioConnection_F32(i16_to_f32_L, 0, output_queue_L, 0);
		patchCord101_R = new AudioConnection_F32(i16_to_f32_R, 0, output_queue_R, 0);
	}
	
	//define audio processing blocks.
    AudioInputUSB  	usb_in;  //from the original Teensy Audio Library, expects Int16 audio data
    AudioConvert_I16toF32   i16_to_f32_L, i16_to_f32_R; 
    AudioRecordQueue_F32    output_queue_L,output_queue_R;    
	
	//define the audio connections
    AudioConnection     *patchCord100_L, *patchCord100_R;
    AudioConnection_F32 *patchCord101_L, *patchCord101_R;
	
    void update(void) {
		//Serial.println("AudioSynthNoiseWhite_F32: update().");
		output_queue_L.begin();
		output_queue_R.begin();
		
		//manually update audio blocks in the desired order
		usb_in.update();  //the output should be routed directly via the AudioConnection
		i16_to_f32_L.update();  // output is routed via the AudioConnection
		i16_to_f32_R.update();  // output is routed via the AudioConnection
		output_queue_L.update();
		output_queue_R.update();

		//handle the output for the left channel
		audio_block_f32_t  *block;
		block = output_queue_L.getAudioBlock();
		if (block == NULL) return;
		AudioStream_F32::transmit(block,0);
		output_queue_L.freeAudioBlock();
		output_queue_L.end();
		
		//handle the output for the left channel
		block = output_queue_R.getAudioBlock();
		if (block == NULL) return;
		AudioStream_F32::transmit(block,1);
		output_queue_R.freeAudioBlock();
		output_queue_R.end();
    }
private:    

};

class AudioOutputUSB_F32 : public AudioStream_F32
{
//GUI: inputs:2, outputs:0 //this line used for automatic generation of GUI node
//GUI: shortName:usbAudioOut  //this line used for automatic generation of GUI node
public:
	AudioOutputUSB_F32() : AudioStream_F32(2, inputQueueArray_f32) {
		makeConnections();
	}
	
	AudioOutputUSB_F32(const AudioSettings_F32 &settings) : AudioStream_F32(2, inputQueueArray_f32) {
		makeConnections();
	}
	
	void makeConnections(void) {
		//make the audio connections
		patchCord100_L = new AudioConnection_F32(queue_L, 0, f32_to_i16_L, 0);  //noise is an Int16 audio object.  So, convert it!
    	patchCord100_R = new AudioConnection_F32(queue_R, 0, f32_to_i16_R, 0);  //noise is an Int16 audio object.  So, convert it!
    	patchCord101_L = new AudioConnection(f32_to_i16_L, 0, usb_out, 0); //Int16 audio connection
		patchCord101_R = new AudioConnection(f32_to_i16_R, 0, usb_out, 1); //Int16 audio connection
	}
	
	//define audio processing blocks.
    AudioPlayQueue_F32    queue_L,queue_R;    
	AudioConvert_F32toI16   f32_to_i16_L, f32_to_i16_R; 
    AudioOutputUSB  	    usb_out; //from the original Teensy Audio Library, expects Int16 audio data
    
	//define the audio connections
    AudioConnection_F32     *patchCord100_L, *patchCord100_R;
    AudioConnection *patchCord101_L, *patchCord101_R;
	
    void update(void) {
		//Serial.println("AudioSynthNoiseWhite_F32: update().");
		//queue_L.begin();
		//queue_R.begin();
		
		//is there audio waiting for us for the left channel?
		audio_block_f32_t *block;
		block = receiveReadOnly_f32(0); 
		if (!block) return; //if no audio, return now.
		
		//there is some audio, so execute the processing chain for the left channel
		queue_L.playAudioBlock(block);
		AudioStream_F32::release(block);
		queue_L.update();
		f32_to_i16_L.update();
		
		//see if there is a right channel
		block = receiveReadOnly_f32(1); 
		if (block) {
			//there is a right channel.  process it now
			queue_R.playAudioBlock(block);
			AudioStream_F32::release(block);
			queue_R.update();
			f32_to_i16_R.update();
		}
		
		//whether or not there was right-channel audio, update the usb_out
		usb_out.update();
		return;
    }
private:    
	audio_block_f32_t *inputQueueArray_f32[2];
};

#endif