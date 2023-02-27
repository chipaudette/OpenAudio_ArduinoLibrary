/*
*   AudioRecordQueue_F32
*
*   Created: Chip Audette (OpenAudio), Feb 2017
*       Extended from on Teensy Audio Library
*
*   License: MIT License.  Use at your own risk.
* Rebuilt Feb 2023 to include stall/non-stall behavior and max buffers.
* This is slightly adapted from the updated play_queue in the I16
* Teensy Audio library.  Thanks to Jonathan Oakley for the improvements.
* See play_queue_f32.h for more details
*/

#include "play_queue_f32.h"

void AudioPlayQueue_F32::setMaxBuffers(uint8_t maxb)
{
  if (maxb < 2)
    maxb = 2 ;
  if (maxb > MAX_BUFFERS)
    maxb = MAX_BUFFERS ;
  max_buffers = maxb ;
}

bool AudioPlayQueue_F32::available(void)
{
        if (userblock) return true;
        userblock = AudioStream_F32::allocate_f32();
        if (userblock) return true;
        return false;
}

/* Get address of current data buffer, newly allocated if necessary.
 * With behaviour == ORIGINAL this will stall (calling yield()) until an audio block
 * becomes available - there's no real guarantee this will ever happen...
 * With behaviour == NON_STALLING this will never stall, and will conform to the published
 * API by returning NULL if no audio block is available.
 * return: NULL if buffer not available, else pointer to buffer
 * of AUDIO_BLOCK_SAMPLES of float32_t
 */
float32_t* AudioPlayQueue_F32::getBuffer(void)
{
	if (NULL == userblock) // not got one: try to get one
	{
		switch (behaviour)
		{
			default:
				while (1)
				{
					userblock = AudioStream_F32::allocate_f32();
					if (userblock)
						break;
					yield();
				}
				break;

			case NON_STALLING:
				userblock = AudioStream_F32::allocate_f32();
				break;
		}
	}

	return userblock == NULL
					?NULL
					:userblock->data;
}

/* Queue userblock for later playback in update().
 * If there's no user block in use then we presume success: this means it's
 * safe to keep calling playBuffer() regularly even if we've not been
 * creating audio to be played.
 * return 0 for success, 1 for re-try required        */
uint32_t AudioPlayQueue_F32::playBuffer(void)
{
	uint32_t result = 0;
	uint32_t h;

	if (userblock) // only need to queue if we have a user block!
	{
		// Find place for next queue entry
		h = head + 1;
		if (h >= max_buffers) h = 0;
		// Wait for space, or return "please re-try", depending on behaviour
		switch (behaviour)
		{
			default:
				while (tail == h); // wait until space in the queue
				break;

			case NON_STALLING:
				if (tail == h)	// if no space...
					result = 1;	// ...return 1: user code must re-try later
				break;
		}
		if (0 == result)
		{
			queue[h] = userblock;	// block is queued for transmission
			head = h;				// head has changed
			userblock = NULL;		// block no longer available for filling
		}
	}
	return result;
}

/* Put a single sample to buffer, and queue if buffer full.
 * return 0 for success; 1: failed, data not stored, call again
 * with same data.                                           */
uint32_t AudioPlayQueue_F32::play(float32_t data)
{
	uint32_t result = 1;
	float32_t* buf = getBuffer();
	do
	{
		if (NULL == buf) // no buffer, failed already
			break;
		if (uptr >= AUDIO_BLOCK_SAMPLES) // buffer is full, we're re-called: try again
		{
			if (0 == playBuffer()) // success emitting old buffer...
			{
				uptr = 0;			// ...start at beginning...
				buf = getBuffer();	// ...of new buffer
				continue;			// loop to check buffer and store the sample
			}
		}
		else // non-full buffer
		{
		  buf [uptr++] = data ;
		  result = 0;
		  if (uptr >= AUDIO_BLOCK_SAMPLES	// buffer is full...
		   && 0 == playBuffer())			// ... try to queue it
			  uptr = 0; // success!
		}
	} while (false);
	return result;
}

/*
 * Put multiple samples to buffer(s), and queue if buffer(s) full.
 * return 0 for success; >0: failed, data not stored, call again with
 *  remaining data (return is unused data length)       */
uint32_t AudioPlayQueue_F32::play(const float32_t *data, uint32_t len)
{
	uint32_t result = len;
	float32_t * buf = getBuffer();

	do
	{
		unsigned int avail_in_userblock = AUDIO_BLOCK_SAMPLES - uptr ;
		unsigned int to_copy = avail_in_userblock > len ? len : avail_in_userblock ;

		if (NULL == buf) // no buffer, failed
			break;
		if (uptr >= AUDIO_BLOCK_SAMPLES) // buffer is full, we're re-called: try again
		{
			if (0 == playBuffer()) // success emitting old buffer...
			{
				uptr = 0;			// ...start at beginning...
				buf = getBuffer();	// ...of new buffer
				continue;			// loop to check buffer and store more samples
			}
		}
		if (0 == len) // nothing left to do
			break;

		// we have a buffer and something to copy to it: do that
		memcpy ((void*)(buf+uptr), (void*)data, to_copy * sizeof(float32_t)) ;
		uptr   += to_copy;
		data   += to_copy;
		len    -= to_copy;
		result -= to_copy;
		if (uptr >= AUDIO_BLOCK_SAMPLES)	// buffer is full...
		{
			if (0 == playBuffer())			// ... try to queue it
			{
				uptr = 0;					// success!
				if (len > 0)				// more to buffer...
					buf = getBuffer();		// ...try to do that
			}
			else
				break;	// queue failed: exit and try again later
		}
	} while (len > 0);
	return result;
}

// Assume user already has an audio_block that was NOT allocated by this
// playBuffer.  Here, you hand it your buffer.  This object takes ownership
// of it and puts it into the queue.  This is not in I16 library.
void AudioPlayQueue_F32::playAudioBlock(audio_block_f32_t *audio_block) {
    uint32_t h;

    if (!audio_block) return;
    h = head + 1;
    if (h >= max_buffers) h = 0;
    while (tail == h) ;       // wait until space in the queue
    queue[h] = audio_block;
    audio_block->ref_count++; //take ownership of this block
    head = h;
    userblock = NULL;
}

void AudioPlayQueue_F32::update(void)
{
    audio_block_f32_t *block;
	uint32_t t;

	t = tail;
	if (t != head) {    // a data block is available to transmit out
		if (++t >= max_buffers) t = 0; // tail is advanced 1, circularly
		block = queue[t];   // pointer to next block
		tail = t;
		AudioStream_F32::transmit(block);
		AudioStream_F32::release(block);	 // we've lost interest in this block...
		queue[t] = NULL; // ...forget it here, too
	}
}
