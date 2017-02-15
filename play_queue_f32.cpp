/* 
*	AudioRecordQueue_F32
*
*	Created: Chip Audette (OpenAudio), Feb 2017
*       Extended from on Teensy Audio Library
*
*	License: MIT License.  Use at your own risk.
*/

#include "play_queue_f32.h"
#include "utility/dspinst.h"

bool AudioPlayQueue_F32::available(void)
{
        if (userblock) return true;
        userblock = allocate_f32();
        if (userblock) return true;
        return false;
}

float32_t * AudioPlayQueue_F32::getBuffer(void)
{
	if (userblock) return userblock->data;
	while (1) {
		userblock = allocate_f32();
		if (userblock) return userblock->data;
		yield();
	}
}

void AudioPlayQueue_F32::playBuffer(void)
{
	uint32_t h;

	if (!userblock) return;
	h = head + 1;
	if (h >= 32) h = 0;
	while (tail == h) ; // wait until space in the queue
	queue[h] = userblock;
	head = h;
	userblock = NULL;
}

void AudioPlayQueue_F32::update(void)
{
	audio_block_f32_t *block;
	uint32_t t;

	t = tail;
	if (t != head) {
		if (++t >= 32) t = 0;
		block = queue[t];
		tail = t;
		transmit(block);
		release(block);
	}
}

//assume user already has an audio_block that was NOT allocated by this
//playBuffer.  Here, you hand it your buffer.  This object takes ownership
//of it and puts it into the queue
void AudioPlayQueue_F32::playAudioBlock(audio_block_f32_t *audio_block) {
	uint32_t h;

	if (!audio_block) return;
	h = head + 1;
	if (h >= 32) h = 0;
	while (tail == h) ; // wait until space in the queue
	queue[h] = audio_block;
	audio_block->ref_count++; //take ownership of this block
	head = h;
	//userblock = NULL;	
	
}
