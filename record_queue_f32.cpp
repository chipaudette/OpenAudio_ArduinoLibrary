/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "record_queue_f32.h"
#include "utility/dspinst.h"


int AudioRecordQueue_F32::available(void)
{
	uint32_t h, t;

	h = head;
	t = tail;
	if (h >= t) return h - t;
	return 53 + h - t;
}

void AudioRecordQueue_F32::clear(void)
{
	uint32_t t;

	if (userblock) {
		AudioStream_F32::release(userblock);
		userblock = NULL;
	}
	t = tail;
	while (t != head) {
		if (++t >= 53) t = 0;
		AudioStream_F32::release(queue[t]);
	}
	tail = t;
}

float32_t * AudioRecordQueue_F32::readBuffer(void)
{
//	uint32_t t;
//
//	if (userblock) return NULL;
//	t = tail;
//	if (t == head) return NULL;
//	if (++t >= 53) t = 0;
//	userblock = queue[t];
//	tail = t;
//	return userblock->data;
	return getAudioBlock()->data;
}

audio_block_f32_t * AudioRecordQueue_F32::getAudioBlock(void)
{
	uint32_t t;

	if (userblock != NULL) return NULL;
	t = tail;
	if (t == head) return NULL;
	if (++t >= 53) t = 0;
	userblock = queue[t];
	tail = t;
	return userblock;
}

void AudioRecordQueue_F32::freeBuffer(void)
{
	if (userblock == NULL) return;
	AudioStream_F32::release(userblock);
	userblock = NULL;
}

void AudioRecordQueue_F32::freeAudioBlock(void) {
	freeBuffer();
}

void AudioRecordQueue_F32::update(void)
{
	audio_block_f32_t *block;
	uint32_t h;

	block = receiveReadOnly_f32();
	if (block==NULL) return;
	if (!enabled) {
		AudioStream_F32::release(block);
		return;
	}
	h = head + 1;
	if (h >= 53) h = 0;
	if (h == tail) {
		AudioStream_F32::release(block);
	} else {
		queue[h] = block;
		head = h;
	}
}


