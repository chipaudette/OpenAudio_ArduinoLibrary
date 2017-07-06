/* 
*	AudioRecordQueue_F32
*
*	Created: Chip Audette (OpenAudio), Feb 2017
*       Extended from on Teensy Audio Library
*
*	License: MIT License.  Use at your own risk.
*/


#ifndef record_queue_f32_h_
#define record_queue_f32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"

class AudioRecordQueue_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:0 //this line used for automatic generation of GUI node
public:
	AudioRecordQueue_F32(void) : AudioStream_F32(1, inputQueueArray),
		userblock(NULL), head(0), tail(0), enabled(0) { }
	AudioRecordQueue_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray),
		userblock(NULL), head(0), tail(0), enabled(0) { }
	void begin(void) {
		clear();
		enabled = 1;
	}
	int available(void);
	void clear(void);
	//int16_t * readBuffer(void);
	float32_t *readBuffer(void);
	audio_block_f32_t *getAudioBlock(void);
	void freeBuffer(void);
	void freeAudioBlock(void);
	void end(void) {
		enabled = 0;
	}
	virtual void update(void);
private:
	audio_block_f32_t *inputQueueArray[1];
	audio_block_f32_t * volatile queue[53];
	audio_block_f32_t *userblock;
	volatile uint8_t head, tail, enabled;
};

#endif
