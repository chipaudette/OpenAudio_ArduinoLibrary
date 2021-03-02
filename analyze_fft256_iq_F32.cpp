/*      analyze_fft_iq_F32.cpp
 *
 * Converted to F32 floating point input and also extended
 * for complex I and Q inputs
 *   * Adapted all I/O to be F32 floating point for OpenAudio_ArduinoLibrary
 *   * Future: Add outputs for I & Q FFT x2 for overlapped FFT
 *   * Windowing None, Hann, Kaiser and Blackman-Harris.
 *
 * Conversion Copyright (c) 2021 Bob Larkin
 * Same MIT license as PJRC:
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
#include "analyze_fft256_iq_F32.h"

// Move audio data from audio_block_f32_t to the interleaved FFT instance buffer.
static void copy_to_fft_buffer1(void *destination, const void *sourceI, const void *sourceQ)  {
    const float *srcI = (const float *)sourceI;
    const float *srcQ = (const float *)sourceQ;
    float *dst = (float *)destination;
    for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
       *dst++ = *srcI++;     // real sample, interleave
       //*dst++ = 0.0f;
       *dst++ = *srcQ++;     // imag
       //*dst++ = 0.0f; 
       }
    }

static void apply_window_to_fft_buffer1(void *fft_buffer, const void *window) {
    float *buf = (float *)fft_buffer;      // 0th entry is real (do window) 1th is imag
    const float *win = (float *)window;
    for (int i=0; i < 256; i++)  {
       buf[2*i] *= *win++;      // real
       buf[2*i + 1] *= *win++;  // imag
       }
    }

void AudioAnalyzeFFT256_IQ_F32::update(void)  {
  audio_block_f32_t *block_i,*block_q;

  block_i = receiveReadOnly_f32(0);
  if (!block_i) return;
  block_q = receiveReadOnly_f32(1);
  if (!block_q)  {
     release(block_i);
     return;
     }
  // Here with two new blocks of data

  // prevblock_i and _q are pointers to the IQ data collected last update()
  if (!prevblock_i || !prevblock_q) {  // Startup
     prevblock_i = block_i;
     prevblock_q = block_q;
     return;  // Nothing to release
     }
  // FFT is 256 and blocks are 128, so we need 2 blocks.  We still do
  // this every 128 samples to get 50% overlap on FFT data to roughly
  // compensate for windowing.
  //                (   dest,            i-source,            q-source   )
  copy_to_fft_buffer1(fft_buffer,     prevblock_i->data, prevblock_q->data);
  copy_to_fft_buffer1(fft_buffer+256, block_i->data,     block_q->data);
  if (pWin)
    apply_window_to_fft_buffer1(fft_buffer, window);
  arm_cfft_radix4_f32(&fft_inst, fft_buffer);  // Finally the FFT

  count++;
  for (int i=0; i < 256; i++) {
     float ss = fft_buffer[2*i]*fft_buffer[2*i] + fft_buffer[2*i+1]*fft_buffer[2*i+1];
     if(count==1)        // Starting new average
        sumsq[i] = ss;
     else if (count <= nAverage)  // Adding on to average
        sumsq[i] += ss;
     }

  if (count >= nAverage) {    // Average is finished
    count = 0;
    float inAf = 1.0f/(float)nAverage;
    for (int i=0; i < 256; i++) {
       int ii = 255 - (i ^ 128);
       if(outputType==FFT_RMS)
          output[ii] = sqrtf(inAf*sumsq[ii]);
       else if(outputType==FFT_POWER)
          output[ii] = inAf*sumsq[ii];
       else if(outputType==FFT_DBFS)
          output[ii] = 10.0f*log10f(inAf*sumsq[ii])-42.1442f;  // Scaled to FS sine wave
       else
          output[ii] = 0.0f;
       }
    }
  outputflag = true;
  release(prevblock_i);    // Release the 2 blocks that were block_i
  release(prevblock_q);    // and block_q on last time through update()
  prevblock_i = block_i;   // We will use these 2 blocks on next update()
  prevblock_q = block_q;   // Just change pointers
}

#if 0
==============================================================

==============================================================
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
// #include "utility/dspinst.h"

// Move audio data from an audio_block_f32_t to the FFT instance buffer.
static void copy_to_fft_buffer(void *destination, const void *source)
{
    const float *src = (const float *)source;
    float *dst = (float *)destination;

    for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
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

void AudioAnalyzeFFT1024_F32::update(void)
{
    audio_block_f32_t *block;
    block = receiveReadOnly_f32();
    if (!block) return;

// What all does 7EM cover??
#if defined(__ARM_ARCH_7EM__)
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
        copy_to_fft_buffer(fft_buffer+0x000, blocklist[0]->data);
        copy_to_fft_buffer(fft_buffer+0x100, blocklist[1]->data);
        copy_to_fft_buffer(fft_buffer+0x200, blocklist[2]->data);
        copy_to_fft_buffer(fft_buffer+0x300, blocklist[3]->data);
        copy_to_fft_buffer(fft_buffer+0x400, blocklist[4]->data);
        copy_to_fft_buffer(fft_buffer+0x500, blocklist[5]->data);
        copy_to_fft_buffer(fft_buffer+0x600, blocklist[6]->data);
        copy_to_fft_buffer(fft_buffer+0x700, blocklist[7]->data);

        if (pWin)
           apply_window_to_fft_buffer(fft_buffer, window);

        arm_cfft_radix4_f32(&fft_inst, fft_buffer);

        for (int i=0; i < 512; i++) {
            float magsq = fft_buffer[2*i]*fft_buffer[2*i] + fft_buffer[2*i+1]*fft_buffer[2*i+1];
            if(outputType==FFT_RMS)
               output[i] = sqrtf(magsq);
            else if(outputType==FFT_POWER)
               output[i] = magsq;
            else if(outputType==FFT_DBFS)
               output[i] = 10.0f*log10f(magsq)-54.1854f;  // Scaled to FS sine wave
            else
               output[i] = 0.0f;
        }
        outputflag = true;
        release(blocklist[0]);
        release(blocklist[1]);
        release(blocklist[2]);
        release(blocklist[3]);
        blocklist[0] = blocklist[4];
        blocklist[1] = blocklist[5];
        blocklist[2] = blocklist[6];
        blocklist[3] = blocklist[7];
        state = 4;
        break;
    }
#else
    release(block);
#endif
}
#endif
