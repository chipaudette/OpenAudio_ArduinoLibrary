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

// Park-Miller-Carta Pseudo-Random Number Generator
// http://www.firstpr.com.au/dsp/rand31/

void AudioSynthNoiseWhite_F32::update(void)
{
	audio_block_t *block;
	audio_block_f32_t *block_f32;
	uint32_t *p, *end;
	int32_t n1, n2, gain;
	uint32_t lo, hi, val1, val2;

	//Serial.println("synth_whitenoise: update()");
	gain = level;
	if (gain == 0) {
		//Serial.println(": Gain = 0, returning.");
		return;
	}
	block = AudioStream::allocate();
	block_f32 = AudioStream_F32::allocate_f32();
	if (!block | !block_f32) {
		//Serial.println(": NULL block. returning.");
		return;
	}
	p = (uint32_t *)(block->data);
	//end = p + AUDIO_BLOCK_SAMPLES/2;
	end = p + (block_f32->length)/2;
	
	lo = seed;
	do {
#if ( defined(KINETISK) ||  defined(__IMXRT1062__) )
		hi = multiply_16bx16t(16807, lo); // 16807 * (lo >> 16)
		lo = 16807 * (lo & 0xFFFF);
		lo += (hi & 0x7FFF) << 16;
		lo += hi >> 15;
		lo = (lo & 0x7FFFFFFF) + (lo >> 31);
		n1 = signed_multiply_32x16b(gain, lo);
		hi = multiply_16bx16t(16807, lo); // 16807 * (lo >> 16)
		lo = 16807 * (lo & 0xFFFF);
		lo += (hi & 0x7FFF) << 16;
		lo += hi >> 15;
		lo = (lo & 0x7FFFFFFF) + (lo >> 31);
		n2 = signed_multiply_32x16b(gain, lo);
		val1 = pack_16b_16b(n2, n1);
		hi = multiply_16bx16t(16807, lo); // 16807 * (lo >> 16)
		lo = 16807 * (lo & 0xFFFF);
		lo += (hi & 0x7FFF) << 16;
		lo += hi >> 15;
		lo = (lo & 0x7FFFFFFF) + (lo >> 31);
		n1 = signed_multiply_32x16b(gain, lo);
		hi = multiply_16bx16t(16807, lo); // 16807 * (lo >> 16)
		lo = 16807 * (lo & 0xFFFF);
		lo += (hi & 0x7FFF) << 16;
		lo += hi >> 15;
		lo = (lo & 0x7FFFFFFF) + (lo >> 31);
		n2 = signed_multiply_32x16b(gain, lo);
		val2 = pack_16b_16b(n2, n1);
		*p++ = val1;
		*p++ = val2;
#elif defined(KINETISL)
		hi = 16807 * (lo >> 16);
		lo = 16807 * (lo & 0xFFFF);
		lo += (hi & 0x7FFF) << 16;
		lo += hi >> 15;
		lo = (lo & 0x7FFFFFFF) + (lo >> 31);
		n1 = signed_multiply_32x16b(gain, lo);
		hi = 16807 * (lo >> 16);
		lo = 16807 * (lo & 0xFFFF);
		lo += (hi & 0x7FFF) << 16;
		lo += hi >> 15;
		lo = (lo & 0x7FFFFFFF) + (lo >> 31);
		n2 = signed_multiply_32x16b(gain, lo);
		val1 = pack_16b_16b(n2, n1);
		*p++ = val1;
#endif
	} while (p < end);
	seed = lo;

	//convert int16 to f32
    #define I16_TO_F32_NORM_FACTOR (3.051757812500000E-05)  //which is 1/32768 
	for (int i=0; i<block_f32->length; i++)
		 block_f32->data[i] = (float32_t)block->data[i] * I16_TO_F32_NORM_FACTOR;
	
	AudioStream_F32::transmit(block_f32);
	AudioStream_F32::release(block_f32);
	AudioStream::release(block);
	//Serial.println(" Done.");
}

uint16_t AudioSynthNoiseWhite_F32::instance_count = 0;


