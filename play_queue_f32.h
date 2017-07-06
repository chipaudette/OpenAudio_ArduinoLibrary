/* 
*	AudioPlayQueue_F32
*
*	Created: Chip Audette (OpenAudio), Feb 2017
*       Extended from on Teensy Audio Library
*
*	License: MIT License.  Use at your own risk.
*/

#ifndef play_queue_f32_h_
#define play_queue_f32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"

class AudioPlayQueue_F32 : public AudioStream_F32
{
//GUI: inputs:0, outputs:1 //this line used for automatic generation of GUI node
public:
	AudioPlayQueue_F32(void) : AudioStream_F32(0, NULL),
		userblock(NULL), head(0), tail(0) { }
	AudioPlayQueue_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL),
		userblock(NULL), head(0), tail(0) { }	
	//void play(int16_t data);
	//void play(const int16_t *data, uint32_t len);
	//void play(float32_t data);
	//void play(const float32_t *data, uint32_t len);
	void playAudioBlock(audio_block_f32_t *);
	bool available(void);
	float32_t * getBuffer(void);
	void playBuffer(void);
	void stop(void);
	//bool isPlaying(void) { return playing; }
	virtual void update(void);
private:
	audio_block_f32_t *queue[32];
	audio_block_f32_t *userblock;
	volatile uint8_t head, tail;
};

#endif
