/*
	Extended to F32
	Created: Chip Audette, OpenAudio, Feb 2017
	Extended to Teensy 4.x Bob larkn June 2020
	
	License: MIT License.  Use at your own risk.
*/

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

#include "synth_whitenoise_f32.h"

// Algorithm from Pure data [noise~] object
// https://github.com/pure-data/pure-data/blob/master/src/d_osc.c

void AudioSynthNoiseWhite_F32::update(void)
{
	float gain = level;
	if (gain == 0) {
		return;
	}

	audio_block_f32_t *block = AudioStream_F32::allocate_f32();
	if (!block) {
		return;
	}
	
	float *p = block->data;
	
	uint32_t val = seed;

	gain *= (float)(1.0 / 0x40000000);

	for(int i = 0; i < block->length; ++i) {
        p[i] = (float)((val & 0x7fffffff) - 0x40000000) * gain;
        val = val * 435898247 + 382842987;
 	}

	seed = val;

	AudioStream_F32::transmit(block);
	AudioStream_F32::release(block);
}

uint16_t AudioSynthNoiseWhite_F32::instance_count = 0;


