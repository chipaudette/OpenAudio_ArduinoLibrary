/*
 *   analyze_fft4096_iq_F32.cpp       Assembled by Bob Larkin   18 Feb 2022
 *
 * External Memory  ****  BETA TEST VERSION - NOT FULLY TESTED **** <<<<<<<<<<
 *
 *  This class is Teensy 4.x ONLY.
 *  F32 Bolocks are always 128 floats, and any data rate is OK.
 *
 * Converted to F32 floating point input and also extended
 * for complex I and Q inputs
 *   * Adapted all I/O to be F32 floating point for OpenAudio_ArduinoLibrary
 *   * Future: Add outputs for I & Q FFT x2 for overlapped FFT
 *   * Windowing None, Hann, Kaiser and Blackman-Harris.
 *
 * Conversion Copyright (c) 2022 Bob Larkin
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

// ***************  TEENSY 4.X ONLY   ****************
#if defined(__IMXRT1062__)

#include <Arduino.h>
#include "analyze_fft4096_iqem_F32.h"

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

void AudioAnalyzeFFT4096_IQEM_F32::update(void)  {
  audio_block_f32_t *block_i,*block_q;
  int i, ii;

  block_i = receiveReadOnly_f32(0);
  if (!block_i) return;
  block_q = receiveReadOnly_f32(1);
  if (!block_q)  {
     release(block_i);
     return;
     }
  // Here with two new blocks of data.  These are retained until the FFT
  // but with new pointers, blocklist_i[] and blocklist_q[].

  // uint32_t tt = micros(); Serial.print(state);

  switch (state) {
  case 0:
      blocklist_i[0] = block_i;  blocklist_q[0] = block_q;  // Copy 2 ptrs
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
      state = 8;
      break;
  case 8:
      blocklist_i[8] = block_i;  blocklist_q[8] = block_q;
      state = 9;
      break;
  case 9:
      blocklist_i[9] = block_i;  blocklist_q[9] = block_q;
      state = 10;
      break;
  case 10:
      blocklist_i[10] = block_i;  blocklist_q[10] = block_q;
      state = 11;
      break;
  case 11:
      blocklist_i[11] = block_i;  blocklist_q[11] = block_q;
      state = 12;
      break;
  case 12:
      blocklist_i[12] = block_i;  blocklist_q[12] = block_q;
      state = 13;
      break;
  case 13:
      blocklist_i[13] = block_i;  blocklist_q[13] = block_q;
      state = 14;
      break;
  case 14:
      blocklist_i[14] = block_i;  blocklist_q[14] = block_q;
      state = 15;
      break;
  case 15:
      blocklist_i[15] = block_i;  blocklist_q[15] = block_q;
      state = 16;
      break;
  //  ********************************************************
  // Once things are running, the loop comes back to this point
  case 16:
      blocklist_i[16] = block_i;  blocklist_q[16] = block_q;

     // Now work on the FFT output data.  This was created in case 31.
     // This next forming of the sumsq[] takes 66 uSec (was 48 uSec with local memory)
     count++;
     for (int i = 0; i < 2048; i++)   {
        // Re-arranging the coefficients. These are bin powers (not Volts)
        // See DD4WH SDR
        float ss0 = *(pFFT_buffer + 2*i) * *(pFFT_buffer + 2*i) +
                    *(pFFT_buffer + 2*i+1) * *(pFFT_buffer + 2*i+1);
        float ss1 = *(pFFT_buffer + 2*(i+2048)) * *(pFFT_buffer + 2*(i+2048)) +
                    *(pFFT_buffer + 2*(i+2048)+1) * *(pFFT_buffer + 2*(i+2048)+1);

        if(!(pSumsq==NULL)) {            // We have memory to do averages
           if(count==1) {                // Starting new average
              *(pSumsq+i+2048) = ss0;
              *(pSumsq+i) = ss1;
              }
           else if (count <= nAverage) { // Adding on to average
              *(pSumsq+i+2048) += ss0;
              *(pSumsq+i) += ss1;
              }
           }
        else                             // No averaging is used
           {
           // Parts of pFFT_buffer are becoming available for
           // temporary storage, but not all:
           *(pFFT_buffer+i) = ss0;
           *(pFFT_buffer+4096+i) = ss1;
           // Now in pFFT_buffer 0,2047 and 4096,6143
           }
        }

      // sumsq[] is filled.  Wait to state==17 to convert to dBFS, etc
      state = 17;
      break;
  case 17:
      blocklist_i[17] = block_i;  blocklist_q[17] = block_q;

     // This state==17 block takes 710 uSec for DBFS, but
     // only 65 for POWER.  DB conversions do not need to be under
     // this interrupt and POWER output should be used if time is short.
     if (pSumsq==NULL || count>=nAverage) { // Average is not being done or is finished
        outputflag = false; // Avoid starting read() during block 17 to 18
        float inAf = 1.0f/(float)nAverage;
        for (ii=0; ii < 2048; ii++) {
            // xAxis, bit 0 left/right;  bit 1 low to high
            if(xAxis & 0X02)
               i = ii;
            else
               i = ii^2048;

            if(xAxis & 0X01)
               i = (4095 - i);

            if(!(pSumsq==NULL)) { // We have memory to do averages
               if(outputType==FFT_RMS)
                  *(pOutput+i) = sqrtf(inAf* *(pSumsq+ii));
               else if(outputType==FFT_POWER)
                  *(pOutput+i) = inAf* *(pSumsq+ii);
               else if(outputType==FFT_DBFS)
                  *(pOutput+i) = 10.0f*log10f(inAf* *(pSumsq+ii))-66.23f; // Scaled to FS sine wave
               else
                  *(pOutput+i) = 0.0f;
               }
            else {               // No averaging
               if(outputType==FFT_RMS)
                  *(pOutput+i) = sqrtf(*(pFFT_buffer+ii));
               else if(outputType==FFT_POWER)
                  *(pOutput+i) = *(pFFT_buffer+ii);
               else if(outputType==FFT_DBFS)
                  *(pOutput+i) = 10.0f*log10f(*(pFFT_buffer+ii))-66.23f;
               }  // End, no averaging
	       }  // End of "over all i"
         }  // end of Average is Finished
      state = 18;
      break;
  case 18:
      blocklist_i[18] = block_i;  blocklist_q[18] = block_q;

     // Second half of post-FFT processing.  dBFS (log10f) is the big user of time.
     if (pSumsq==NULL || count>=nAverage) {    // Average is finished
        count = 0;
        float inAf = 1.0f/(float)nAverage;
        // ii is the index to data source, i is for data output
        for (int ii=2048; ii < 4096; ii++) {
            // xAxis, bit 0 left/right;  bit 1 low to high
            if(xAxis & 0X02)
               i = ii;
            else
               i = ii^2048;

            if(xAxis & 0X01)
               i = (4095 - i);

            if(!(pSumsq==NULL)) {     // We have memory to do averages
                if(outputType==FFT_RMS)
                   *(pOutput+i) = sqrtf(inAf* *(pSumsq+ii));
                else if(outputType==FFT_POWER)
                   *(pOutput+i) = inAf* *(pSumsq+ii);
                else if(outputType==FFT_DBFS)
                   *(pOutput+i) = 10.0f*log10f(inAf* *(pSumsq+ii))-66.23f;  // Scaled to FS sine wave
                else
                   *(pOutput+i) = 0.0f;
                }
            else {                     // No averaging being done
               if(outputType==FFT_RMS)
                  *(pOutput+i) = sqrtf(*(pFFT_buffer+ii+2048));
               else if(outputType==FFT_POWER)
                  *(pOutput+i) = *(pFFT_buffer+ii+2048);
               else if(outputType==FFT_DBFS)
                  *(pOutput+i) = 10.0f*log10f(*(pFFT_buffer+ii+2048))-66.23f;
               else
                   *(pOutput+i) = 0.0f;
                }
            }
            outputflag = true;
        }  // end of Average is Finished
      state = 19;
      break;
  case 19:
      blocklist_i[19] = block_i;  blocklist_q[19] = block_q;
      state = 20;
      break;
  case 20:
      blocklist_i[20] = block_i;  blocklist_q[20] = block_q;
      state = 21;
      break;
  case 21:
      blocklist_i[21] = block_i;  blocklist_q[21] = block_q;
      state = 22;
      break;
  case 22:
      blocklist_i[22] = block_i;  blocklist_q[22] = block_q;
      state = 23;
      break;
  case 23:
      blocklist_i[23] = block_i;  blocklist_q[23] = block_q;
      state = 24;
      break;
  case 24:
      blocklist_i[24] = block_i;  blocklist_q[24] = block_q;
      state = 25;
      break;
  case 25:
      blocklist_i[25] = block_i;  blocklist_q[25] = block_q;
      state = 26;
      break;
  case 26:
      blocklist_i[26] = block_i;  blocklist_q[26] = block_q;
      state = 27;
      break;
  case 27:
      blocklist_i[27] = block_i;  blocklist_q[27] = block_q;
      state = 28;
      break;
  case 28:
      blocklist_i[28] = block_i;  blocklist_q[28] = block_q;
      state = 29;
      break;
  case 29:
      blocklist_i[29] = block_i;  blocklist_q[29] = block_q;
      state = 30;
      break;
  case 30:
      blocklist_i[30] = block_i;  blocklist_q[30] = block_q;
      state = 31;
      break;
  case 31:
      blocklist_i[31] = block_i;  blocklist_q[31] = block_q;

      // Copy 8192 data to fft_buffer This state==31 takes about 530 uSec, including the FFT.
      // i & q interleaved data.
      copy_to_fft_buffer1(pFFT_buffer+0x000, blocklist_i[0]->data, blocklist_q[0]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x100, blocklist_i[1]->data, blocklist_q[1]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x200, blocklist_i[2]->data, blocklist_q[2]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x300, blocklist_i[3]->data, blocklist_q[3]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x400, blocklist_i[4]->data, blocklist_q[4]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x500, blocklist_i[5]->data, blocklist_q[5]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x600, blocklist_i[6]->data, blocklist_q[6]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x700, blocklist_i[7]->data, blocklist_q[7]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x800, blocklist_i[8]->data, blocklist_q[8]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x900, blocklist_i[9]->data, blocklist_q[9]->data);
      copy_to_fft_buffer1(pFFT_buffer+0xA00, blocklist_i[10]->data, blocklist_q[10]->data);
      copy_to_fft_buffer1(pFFT_buffer+0xB00, blocklist_i[11]->data, blocklist_q[11]->data);
      copy_to_fft_buffer1(pFFT_buffer+0xC00, blocklist_i[12]->data, blocklist_q[12]->data);
      copy_to_fft_buffer1(pFFT_buffer+0xD00, blocklist_i[13]->data, blocklist_q[13]->data);
      copy_to_fft_buffer1(pFFT_buffer+0xE00, blocklist_i[14]->data, blocklist_q[14]->data);
      copy_to_fft_buffer1(pFFT_buffer+0xF00, blocklist_i[15]->data, blocklist_q[15]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1000, blocklist_i[16]->data, blocklist_q[16]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1100, blocklist_i[17]->data, blocklist_q[17]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1200, blocklist_i[18]->data, blocklist_q[18]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1300, blocklist_i[19]->data, blocklist_q[19]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1400, blocklist_i[20]->data, blocklist_q[20]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1500, blocklist_i[21]->data, blocklist_q[21]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1600, blocklist_i[22]->data, blocklist_q[22]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1700, blocklist_i[23]->data, blocklist_q[23]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1800, blocklist_i[24]->data, blocklist_q[24]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1900, blocklist_i[25]->data, blocklist_q[25]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1A00, blocklist_i[26]->data, blocklist_q[26]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1B00, blocklist_i[27]->data, blocklist_q[27]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1C00, blocklist_i[28]->data, blocklist_q[28]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1D00, blocklist_i[29]->data, blocklist_q[29]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1E00, blocklist_i[30]->data, blocklist_q[30]->data);
      copy_to_fft_buffer1(pFFT_buffer+0x1F00, blocklist_i[31]->data, blocklist_q[31]->data);


    // Apply the window function, if any, to the time series.  Half size window buffer.
    if(wNum>NO_WINDOW  &&  pWindow)    // fixed syntax 14May2022 RSL
      {
      for (int i=0; i < 2048; i++)  {
         *(pFFT_buffer + 2*i) *= *(pWindow + i);      // real
         *(pFFT_buffer + 2*i+1) *= *(pWindow + i);    // imag
         }
      for (int i=0; i < 2048; i++)  {                 // Second half
         *(pFFT_buffer + 8191 - 2*i) *= *(pWindow + i);
         *(pFFT_buffer + 8190 - 2*i) *= *(pWindow + i);
         }
      }

      // Teensyduino core for T4.x supports arm_cfft_f32
      // arm_cfft_f32 (const arm_cfft_instance_f32 *S, float32_t *p1,
      //                     uint8_t ifftFlag, uint8_t bitReverseFlag)
      // I & O are real/imag interleaved in 8192-float point array p1.
      arm_cfft_f32(&Sfft, pFFT_buffer, 0, 1);

      release(blocklist_i[0]);  release(blocklist_q[0]);
      release(blocklist_i[1]);  release(blocklist_q[1]);
      release(blocklist_i[2]);  release(blocklist_q[2]);
      release(blocklist_i[3]);  release(blocklist_q[3]);
      release(blocklist_i[4]);  release(blocklist_q[4]);
      release(blocklist_i[5]);  release(blocklist_q[5]);
      release(blocklist_i[6]);  release(blocklist_q[6]);
      release(blocklist_i[7]);  release(blocklist_q[7]);
      release(blocklist_i[8]);  release(blocklist_q[8]);
      release(blocklist_i[9]);  release(blocklist_q[9]);
      release(blocklist_i[10]);  release(blocklist_q[10]);
      release(blocklist_i[11]);  release(blocklist_q[11]);
      release(blocklist_i[12]);  release(blocklist_q[12]);
      release(blocklist_i[13]);  release(blocklist_q[13]);
      release(blocklist_i[14]);  release(blocklist_q[14]);
      release(blocklist_i[15]);  release(blocklist_q[15]);

      blocklist_i[0] = blocklist_i[16];
      blocklist_i[1] = blocklist_i[17];
      blocklist_i[2] = blocklist_i[18];
      blocklist_i[3] = blocklist_i[19];
      blocklist_i[4] = blocklist_i[20];
      blocklist_i[5] = blocklist_i[21];
      blocklist_i[6] = blocklist_i[22];
      blocklist_i[7] = blocklist_i[23];
      blocklist_i[8] = blocklist_i[24];
      blocklist_i[9] = blocklist_i[25];
      blocklist_i[10] = blocklist_i[26];
      blocklist_i[11] = blocklist_i[27];
      blocklist_i[12] = blocklist_i[28];
      blocklist_i[13] = blocklist_i[29];
      blocklist_i[14] = blocklist_i[30];
      blocklist_i[15] = blocklist_i[31];

      blocklist_q[0] = blocklist_q[16];
      blocklist_q[1] = blocklist_q[17];
      blocklist_q[2] = blocklist_q[18];
      blocklist_q[3] = blocklist_q[19];
      blocklist_q[4] = blocklist_q[20];
      blocklist_q[5] = blocklist_q[21];
      blocklist_q[6] = blocklist_q[22];
      blocklist_q[7] = blocklist_q[23];
      blocklist_q[8] = blocklist_q[24];
      blocklist_q[9] = blocklist_q[25];
      blocklist_q[10] = blocklist_q[26];
      blocklist_q[11] = blocklist_q[27];
      blocklist_q[12] = blocklist_q[28];
      blocklist_q[13] = blocklist_q[29];
      blocklist_q[14] = blocklist_q[30];
      blocklist_q[15] = blocklist_q[31];

      state = 16;
      break;       // From case 31
    } // End of switch & case 31
    // Serial.print(",");  Serial.println(micros() - tt);
  }  // End update()
  // End, if Teensy 4.x
#endif
