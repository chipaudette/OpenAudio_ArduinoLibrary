/*
 * analyze_tonedetect_F32.cpp  Converted to float from PJRC Teensy Audio Library
 * for the OpenAudio_TeensyArduino library (floating point audio).
 *   MIT License on changed portions
 *   Bob Larkin March 2021
 *
 * See also  analyze_CTCSS_F32  that is specific for the CTCSS tone system
 * with tones in the 67.0 to 250.3 Hz range.  (In this same library)
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

#include <Arduino.h>
#include "analyze_tonedetect_F32.h"
//#include "utility/dspinst.h"

void AudioAnalyzeToneDetect_F32::update(void)  {
    audio_block_f32_t *block;
    float q0, q1, q2, coef;
    float *p, *end;
    uint16_t n;

    block = receiveReadOnly_f32();
    if (!block) return;
    if (!enabled) {
        release(block);
        return;
        }

    p = block->data;
    end = p + AUDIO_BLOCK_SAMPLES;
    n = count;
    coef = coefficient;
    q1 = s1;
    q2 = s2;
    if(n==length) {q1=0.0f; q2=0.0; }
    do {                    // Update Goertzel algorithm
        q0 = (*p++) + coef*q1 - q2;
        q2 = q1;
        q1 = q0;
        if (--n == 0) {  // Full "count" been achieved, estimate done
            out1 = q1;
            out2 = q2;
            q1 = 0.0f;
            q2 = 0.0f;
            new_output = true;
            n = length;
            }
       } while (p < end);
    count = n;
    s1 = q1;
    s2 = q2;
    release(block);
    }

void AudioAnalyzeToneDetect_F32::set_params(float coef, uint16_t cycles, uint16_t len)  {
    __disable_irq();
    coefficient = coef;
    ncycles = cycles;
    length = len;
    count = len;
    s1 = 0.0f;
    s2 = 0.0f;
    enabled = true;
    __enable_irq();
    }

float AudioAnalyzeToneDetect_F32::read(void)  {
    float coef, q1, q2, power;
    uint16_t len;

    __disable_irq();
    coef = coefficient;
    q1 = out1;
    q2 = out2;
    len = length;
    __enable_irq();
    power = q1*q1 + q2*q2 - q1*q2*coef;
    return 2.0f*gain*sqrtf(power)/(float)len; // Scaled to gain*(0.0, 1.0)
    }

AudioAnalyzeToneDetect_F32::operator bool()  {
    float coef, q1, q2, power, trigger;
    uint16_t len;

    __disable_irq();
    coef = coefficient;
    q1 = out1;
    q2 = out2;
    len = length;
    __enable_irq();
    power = gain*gain*(q1*q1 + q2*q2 - q1*q2*coef);
    trigger = (float)len * thresh;
    trigger *= trigger;
    //Serial.println("bool: power, trigger = "); Serial.print(power, 6); Serial.print(", "); Serial.println(trigger, 6);
    return (power >= trigger);
    // TODO: this should really remember if it's retuned true previously,
    // so it can give a single true response each time a tone is seen.
    }
