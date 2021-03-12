/*      analyze_fft256_iq_F32.cpp
 *
 * Converted to F32 floating point input and also extended
 * for complex I and Q inputs
 *   * Adapted all I/O to be F32 floating point for OpenAudio_ArduinoLibrary
 *   * Future: Add outputs for I & Q FFT x2 for overlapped FFT
 *   * Windowing None, Hann, Kaiser and Blackman-Harris.
 *  See analyze_fft256_iq_F32. for more info.
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
static void copy_to_fft_buffer0(void *destination, const void *sourceI, const void *sourceQ)  {
    const float *srcI = (const float *)sourceI;
    const float *srcQ = (const float *)sourceQ;
    float *dst = (float *)destination;
    for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
       *dst++ = *srcI++;     // real sample, interleave
       *dst++ = *srcQ++;     // imag
       }
    }

static void apply_window_to_fft_buffer1(void *fft_buffer, const void *window) {
    float *buf = (float *)fft_buffer;      // 0th entry is real (do window) 1th is imag
    const float *win = (float *)window;
    for (int i=0; i < 256; i++)  {
       buf[2*i] *= *win;      // real
       buf[2*i + 1] *= *win++;  // imag
       }
    }

void AudioAnalyzeFFT256_IQ_F32::update(void)  {
  audio_block_f32_t *block_i,*block_q;
  int ii;

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
  copy_to_fft_buffer0(fft_buffer,     prevblock_i->data, prevblock_q->data);
  copy_to_fft_buffer0(fft_buffer+256, block_i->data,     block_q->data);
  if (pWin)
    apply_window_to_fft_buffer1(fft_buffer, window);

#if defined(__IMXRT1062__)
  // Teensyduino core for T4.x supports arm_cfft_f32
  // arm_cfft_f32 (const arm_cfft_instance_f32 *S, float32_t *p1, uint8_t ifftFlag, uint8_t bitReverseFlag)
  arm_cfft_f32(&Sfft, fft_buffer, 0, 1);
#else
  // For T3.x go back to old (deprecated) style
  arm_cfft_radix4_f32(&fft_inst, fft_buffer);
#endif

  count++;
  for (int i = 0; i < 128; i++)   {
     // From complex FFT the "negative frequencies" are mirrors of the frequencies above fs/2.  So, we get
     // frequencies from 0 to fs by re-arranging the coefficients. These are powers (not Volts)
     // See DD4WH SDR  (Note - here and at "if(xAxis & xxxx)" below, we may have redundancy in index changing.
     // Leave as is for now.)
     float ss0 = fft_buffer[2 * i] *     fft_buffer[2 * i] +
                 fft_buffer[2 * i + 1] * fft_buffer[2 * i + 1];
     float ss1 = fft_buffer[2 * (i + 128)] *     fft_buffer[2 * (i + 128)] +
                 fft_buffer[2 * (i + 128) + 1] * fft_buffer[2 * (i + 128) + 1];

     if(count==1) {       // Starting new average
        sumsq[i+128] = ss0;
        sumsq[i] = ss1;
        }
    else if (count <= nAverage) { // Adding on to average
        sumsq[i+128] += ss0;
        sumsq[i] += ss1;
        }
     }

  if (count >= nAverage) {    // Average is finished
     count = 0;
     float inAf = 1.0f/(float)nAverage;
     for (int i=0; i < 256; i++) {
         // xAxis, bit 0 left/right;  bit 1 low to high
         if(xAxis & 0X02)
            ii = i;
         else
            ii = i^128;
         if(xAxis & 0X01)
            ii = (255 - ii);
    if(outputType==FFT_RMS)
       output[i] = sqrtf(inAf*sumsq[ii]);
    else if(outputType==FFT_POWER)
       output[i] = inAf*sumsq[ii];

    else if(outputType==FFT_DBFS)  {
       if(sumsq[i]>0.0f)
          output[i] = 10.0f*log10f(inAf*sumsq[ii]) - 42.144f;  // Scaled to FS sine wave
       else
          output[i] = -193.0f;   // lsb for 23 bit mantissa
       }
    else
       output[i] = 0.0f;
    }    // End, set output[i] over all 512
    outputflag = true;    // moved; rev10mar2021
  }    // End of average is finished
  release(prevblock_i);    // Release the 2 blocks that were block_i
  release(prevblock_q);    // and block_q on last time through update()
  prevblock_i = block_i;   // We will use these 2 blocks on next update()
  prevblock_q = block_q;   // Just change pointers
  }
