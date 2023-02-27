/*
 *   AudioPlayQueue_F32
 *
 *   Created: Chip Audette (OpenAudio), Feb 2017
 *       Extended from Audio Library for Teensy 3.X
 *   License: MIT License.  Use at your own risk.
 *
 * Rebuilt Feb 2023 to include stall/non-stall behavior and max buffers.
 * This is slightly adapted from the updated play_queue in the I16
 * Teensy Audio library.  Thanks toJonathan Oakley for the improvements.
 * Bob Larkin bob@janbob.com
 *
 *     Audio Library for Teensy 3.X
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

#ifndef play_queue_f32_h_
#define play_queue_f32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"

class AudioPlayQueue_F32 : public AudioStream_F32
{
//GUI: inputs:0, outputs:1 //this line used for automatic generation of GUI node
private:
#if defined(__IMXRT1062__) || defined(__MK66FX1M0__) || defined(__MK64FX512__)
	static const unsigned int MAX_BUFFERS = 80;
#else
	static const unsigned int MAX_BUFFERS = 32;
#endif

public:
    AudioPlayQueue_F32(void) : AudioStream_F32(0, NULL),
        userblock(NULL), uptr(0), head(0), tail(0), max_buffers(MAX_BUFFERS) { }
    AudioPlayQueue_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL),
        userblock(NULL), uptr(0), head(0), tail(0), max_buffers(MAX_BUFFERS) { }

	uint32_t play(float32_t data);
	uint32_t play(const float32_t *data, uint32_t len);
	void playAudioBlock(audio_block_f32_t *audio_block);
	bool available(void);
	// Returns a pointer to an array of AUDIO_BLOCK_SAMPLES (usually 128)
	// float32_t. This buffer is within the audio library memory pool.
	// Only a single buffer is allocated at any one time: repeated calls
	// to getBuffer() without calling playBuffer() will yield the same address.
	float32_t * getBuffer(void);
	uint32_t playBuffer(void);
	void stop(void);
	void setMaxBuffers(uint8_t);
	virtual void update(void);
	enum behaviour_e {ORIGINAL,NON_STALLING};
	void setBehaviour(behaviour_e behave)
	   {
	   behaviour = behave;
	   }
private:
	audio_block_f32_t *queue[MAX_BUFFERS];
	audio_block_f32_t *userblock;
	unsigned int uptr; // actually an index, NOT a pointer!
	volatile uint8_t head, tail;
	volatile uint8_t max_buffers;
	behaviour_e behaviour;
};
#endif
