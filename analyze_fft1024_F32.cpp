/* analyze_fft1024_F32.cpp      Converted from Teensy I16 Audio Library
 * This version uses float F32 inputs.  See comments at analyze_fft1024_F32.h
 *
 * Conversion parts copyright (c) Bob Larkin 2021
 *
 *  Audio Library for Teensy 3.X
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
#include "analyze_fft1024_F32.h"

// Move audio data from an audio_block_f32_t to the FFT instance buffer.
// This is for 128 numbers per block
static void copy_to_fft_buffer(void *destination, const void *source)
{
    const float *src = (const float *)source;
    float *dst = (float *)destination;

    for (int i=0; i < 128; i++) {
        *dst++ = *src++;     // real sample
        *dst++ = 0.0f;       // 0 for Imag
    }
}

static void apply_window_to_fft_buffer(void *buffer, const void *window)
{
    float *buf = (float *)buffer;      // 0th entry is real (do window) 1th is imag
    const float *win = (float *)window;

    for (int i=0; i < 1024; i++)
        buf[2*i] *= *win++; 
}

void AudioAnalyzeFFT1024_F32::update(void)  {
    audio_block_f32_t *block;
    uint32_t tt;

    block = AudioStream_F32::receiveReadOnly_f32();
    if (!block) return;

    // tt=micros();

    switch (state) {
    case 0:
        blocklist[0] = block;
        state = 1;
        break;
    case 1:
        blocklist[1] = block;
        state = 2;
        break;
    case 2:
        blocklist[2] = block;
        state = 3;
        break;
    case 3:
        blocklist[3] = block;
        state = 4;
        break;
    case 4:
        blocklist[4] = block;
        state = 5;
        break;
    case 5:
        blocklist[5] = block;
        state = 6;
        break;
    case 6:
        blocklist[6] = block;
        state = 7;
        break;
    case 7:
        blocklist[7] = block;
        // We have 4 previous blocks pointed to by blocklist[]:
        copy_to_fft_buffer(fft_buffer+0x000, blocklist[0]->data);
        copy_to_fft_buffer(fft_buffer+0x100, blocklist[1]->data);
        copy_to_fft_buffer(fft_buffer+0x200, blocklist[2]->data);
        copy_to_fft_buffer(fft_buffer+0x300, blocklist[3]->data);
        // and 4 new blocks, just gathered:
        copy_to_fft_buffer(fft_buffer+0x400, blocklist[4]->data);
        copy_to_fft_buffer(fft_buffer+0x500, blocklist[5]->data);
        copy_to_fft_buffer(fft_buffer+0x600, blocklist[6]->data);
        copy_to_fft_buffer(fft_buffer+0x700, blocklist[7]->data);
 
        if (pWin)
           apply_window_to_fft_buffer(fft_buffer, window);

#if defined(__IMXRT1062__)
        // Teensyduino core for T4.x supports arm_cfft_f32
        // arm_cfft_f32 (const arm_cfft_instance_f32 *S, float32_t *p1, uint8_t ifftFlag, uint8_t bitReverseFlag)
        arm_cfft_f32 (&Sfft, fft_buffer, 0, 1);
#else
        // For T3.x go back to old (deprecated) style
        arm_cfft_radix4_f32(&fft_inst, fft_buffer);
#endif

        count++;      // Next do non-coherent averaging
        for (int i=0; i < 512; i++) {
            float magsq = fft_buffer[2*i]*fft_buffer[2*i] + fft_buffer[2*i+1]*fft_buffer[2*i+1];
            if(count==1)               // Starting new average
               sumsq[i] = magsq;
            else if (count<=nAverage)  // Adding on to average
               sumsq[i] += magsq;
            }
        if (count >= nAverage) {       // Average is finished
            count = 0;
            float inAf = 1.0f/(float)nAverage;
            for(int i=0; i<512; i++)  {
               if(outputType==FFT_RMS)
                  output[i] = sqrtf(inAf*sumsq[i]);
               else if(outputType==FFT_POWER)
                  output[i] = inAf*sumsq[i];
               else if(outputType==FFT_DBFS)  {
                  if(sumsq[i]>0.0f)
                     output[i] = 10.0f*log10f(inAf*sumsq[i])-54.1854f;  // Scaled to FS sine wave
                  else
                     output[i] = -193.0f;   // lsb for 23 bit mantissa
                  }
               else
                  output[i] = 0.0f;
               }    // End, set output[i] over all 512

            outputflag = true;
            }    // End of average is finished

        AudioStream_F32::release(blocklist[0]);
        AudioStream_F32::release(blocklist[1]);
        AudioStream_F32::release(blocklist[2]);
        AudioStream_F32::release(blocklist[3]);

        blocklist[0] = blocklist[4];
        blocklist[1] = blocklist[5];
        blocklist[2] = blocklist[6];
        blocklist[3] = blocklist[7];
        state = 4;
        break;
     }     // End switch(state)
     // Serial.print("uSec: "); Serial.println(micros()-tt);
  }        // End update()
