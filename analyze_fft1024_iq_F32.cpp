/*
 *   analyze_fft1024_iq_F32.cpp       Assembled by Bob Larkin   3 Mar 2021
 * Rev 6 Mar 2021 - Added setXAxis()
 * Rev 10 Mar 2021 Corrected averaging bracket - Bob L
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
#include "analyze_fft1024_iq_F32.h"

// Note: Suppports block size of 128 only.  Very "built in."

// Move audio data from audio_block_f32_t to the interleaved FFT instance buffer.
static void copy_to_fft_buffer1(void *destination, const void *sourceI, const void *sourceQ)  {
    const float *srcI = (const float *)sourceI;
    const float *srcQ = (const float *)sourceQ;
    float *dst = (float *)destination;  // part of fft_buffer array. 256 floats per call
    for (int i=0; i < 128; i++) {
       *dst++ = *srcI++;     // real sample, interleave
       *dst++ = *srcQ++;     // imag
       }
    }

static void apply_window_to_fft_buffer1(void *fft_buffer, const void *window) {
    float *buf = (float *)fft_buffer;      // 0th entry is real (do window) 1st is imag
    const float *win = (float *)window;
    for (int i=0; i < 1024; i++)  {
       buf[2*i] *= *win;      // real
       buf[2*i + 1] *= *win++;  // imag
       }
    }

void AudioAnalyzeFFT1024_IQ_F32::update(void)  {
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

  switch (state) {
  case 0:
      blocklist_i[0] = block_i;  blocklist_q[0] = block_q;
      state = 1;
      break;
  case 1:
      blocklist_i[1] = block_i;  blocklist_q[1] = block_q;
      state = 2;
      break;
  case 2:
      blocklist_i[2] = block_i;  blocklist_q[2] = block_q;
      state = 3;
      break;
  case 3:
      blocklist_i[3] = block_i;  blocklist_q[3] = block_q;
      state = 4;
      break;
  case 4:
      blocklist_i[4] = block_i;  blocklist_q[4] = block_q;
      state = 5;
      break;
  case 5:
      blocklist_i[5] = block_i;  blocklist_q[5] = block_q;
      state = 6;
      break;
  case 6:
      blocklist_i[6] = block_i;  blocklist_q[6] = block_q;
      state = 7;
      break;
  case 7:
      blocklist_i[7] = block_i;  blocklist_q[7] = block_q;
      copy_to_fft_buffer1(fft_buffer+0x000, blocklist_i[0]->data, blocklist_q[0]->data);
      copy_to_fft_buffer1(fft_buffer+0x100, blocklist_i[1]->data, blocklist_q[1]->data);
      copy_to_fft_buffer1(fft_buffer+0x200, blocklist_i[2]->data, blocklist_q[2]->data);
      copy_to_fft_buffer1(fft_buffer+0x300, blocklist_i[3]->data, blocklist_q[3]->data);
      copy_to_fft_buffer1(fft_buffer+0x400, blocklist_i[4]->data, blocklist_q[4]->data);
      copy_to_fft_buffer1(fft_buffer+0x500, blocklist_i[5]->data, blocklist_q[5]->data);
      copy_to_fft_buffer1(fft_buffer+0x600, blocklist_i[6]->data, blocklist_q[6]->data);
      copy_to_fft_buffer1(fft_buffer+0x700, blocklist_i[7]->data, blocklist_q[7]->data);
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
     for (int i = 0; i < 512; i++)   {
        // From complex FFT the "negative frequencies" are mirrors of the frequencies above fs/2.  So, we get
        // frequencies from 0 to fs by re-arranging the coefficients. These are powers (not Volts)
        // See DD4WH SDR
        float ss0 = fft_buffer[2 * i] *     fft_buffer[2 * i] +
                    fft_buffer[2 * i + 1] * fft_buffer[2 * i + 1];
        float ss1 = fft_buffer[2 * (i + 512)] *     fft_buffer[2 * (i + 512)] +
                    fft_buffer[2 * (i + 512) + 1] * fft_buffer[2 * (i + 512) + 1];

        if(count==1) {       // Starting new average
           sumsq[i+512] = ss0;
           sumsq[i] = ss1;
           }
        else if (count <= nAverage) { // Adding on to average
           sumsq[i+512] += ss0;
           sumsq[i] += ss1;
           }
        }

     if (count >= nAverage) {    // Average is finished
        count = 0;
        float inAf = 1.0f/(float)nAverage;
        for (int i=0; i < 1024; i++) {
            // xAxis, bit 0 left/right;  bit 1 low to high
            if(xAxis & 0X02)
               ii = i;
            else
               ii = i^512;
            if(xAxis & 0X01)
               ii = (1023 - ii);

            if(outputType==FFT_RMS)
               output[i] = sqrtf(inAf*sumsq[ii]);
            else if(outputType==FFT_POWER)
               output[i] = inAf*sumsq[ii];
            else if(outputType==FFT_DBFS)  {
               if(sumsq[i]>0.0f)
                  output[i] = 10.0f*log10f(inAf*sumsq[ii])-54.1854f;  // Scaled to FS sine wave
               else
                  output[i] = -193.0f;   // lsb for 23 bit mantissa
               }
            else
               output[i] = 0.0f;
            }    // End, set output[i] over all 512
         outputflag = true;    // moved; rev10mar2021
         }    // End of average is finished

       release(blocklist_i[0]);  release(blocklist_q[0]);
       release(blocklist_i[1]);  release(blocklist_q[1]);
       release(blocklist_i[2]);  release(blocklist_q[2]);
       release(blocklist_i[3]);  release(blocklist_q[3]);

       blocklist_i[0] = blocklist_i[4];
       blocklist_i[1] = blocklist_i[5];
       blocklist_i[2] = blocklist_i[6];
       blocklist_i[3] = blocklist_i[7];
       blocklist_q[0] = blocklist_q[4];
       blocklist_q[1] = blocklist_q[5];
       blocklist_q[2] = blocklist_q[6];
       blocklist_q[3] = blocklist_q[7];
       state = 4;
       break;       // From case 7
    }  // End of switch & case 7
  }  // End update()
