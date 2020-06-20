/*
 * This is a direct translation of the Teensy Audio Library RMS Analyze
 * to use floating point f32.  This is intended to be compatible with
 * Chip Audette's floating point libraries.
 * Bob Larkin 23 April 2020  -  AudioAnalyze_RMS_F32
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
#include "analyze_rms_f32.h"

void AudioAnalyzeRMS_F32::update(void) {
    float32_t *p;
    float32_t sum = 0.0f;
    audio_block_f32_t *blockIn;

    blockIn = AudioStream_F32::receiveReadOnly_f32();
    if (!blockIn) {
        if(errorPrint)  Serial.println("AN_RMS ERR: No input");
        // count++;   // Why ++?  We are not adding to accum
        return;
    }

    p = blockIn->data;
    // Find sum of powers for full vector, see
    // https://github.com/ARM-software/CMSIS/blob/master/CMSIS/DSP_Lib/Source/BasicMathFunctions/arm_dot_prod_f32.c
    // arguments are (source vector 1,  source vector 2,  number of samples,  pointer to f32 out)
    arm_dot_prod_f32(p, p, block_size, &sum);
    accum += (double)sum;
    count++;
    AudioStream_F32::release(blockIn);
}

float AudioAnalyzeRMS_F32::read(void)  {
    __disable_irq();
    // It is safe to convert back to float now
    float32_t sum = (float32_t)accum;
    accum = 0.0;
    float32_t num = (float32_t)count;
    count = 0;
    __enable_irq();
    return sqrtf(sum / (num * (float32_t)block_size));
}
