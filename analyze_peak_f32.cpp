/*
 * This is a direct translation of the Teensy Audio Library Peak Analyze
 * to use floating point f32.  This is intended to be compatible with
 * Chip Audette's floating point libraries.
 * Bob Larkin 23 April 2020  -  AudioAnalyze_Peak_F32
 *
 * Regard the Paul Stoffregen copyright and licensing:
 *
 * Audio Library for Teensy 3.X
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
#include "AudioStream_F32.h"
#include "analyze_peak_f32.h"

void AudioAnalyzePeak_F32::update(void) {
    audio_block_f32_t *blockIn;
    const float32_t *p, *end;
	float32_t min, max, d;

    blockIn = AudioStream_F32::receiveReadOnly_f32();
    if (!blockIn) {
        if(errorPrint)  Serial.println("AN_PEAK ERR: No input memory");
        return;
    }

    // Variable just_read allows both read() and readPeakToPeak() to be used
    if(just_read) {
		// Start new collection of min_sample and max_sample
		min_sample = blockIn->data[0];
        max_sample = blockIn->data[0];
    }
    
	p = blockIn->data;
	end = p + block_size;
	min = min_sample;
	max = max_sample;
	do {
		d=*p++;
		if (d<min) min=d;
		if (d>max) max=d;
	} while (p < end);
	min_sample = min;
	max_sample = max;
	new_output = true;    // Tell available() that data is available
    AudioStream_F32::release(blockIn);
 }

float AudioAnalyzePeak_F32::read(void)  {
	    // Tell update to start new
	    just_read = true;
        __disable_irq();
        float32_t min = min_sample;
        float32_t max = max_sample;
        __enable_irq();
        min = abs(min);
        max = abs(max);
        if (min > max) max = min;
        return max;
    }
    
float AudioAnalyzePeak_F32:: readPeakToPeak(void) {
	    just_read = true;
        __disable_irq();
        float32_t min = min_sample;
        float32_t max = max_sample;
        __enable_irq();
        return (max - min);
    }
