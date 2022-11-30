/*
 *         radioFT8Demodulator_F32.cpp
 * Assembled by Bob Larkin   8 Sept 2022
 * Comments corrected 16 Nov 2022
 *
 *  This class is Teensy 4.x ONLY.
 *
 * Copyright (c) 2022 Bob Larkin
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

/* Sampling (for 96 kHz case):
 *   Every 5 audio samples at 96 kHz produce 1 19.2 kHz sample
 *   Every 3 audio samples at 19.2   produce 1  6.4 kHz sample
 *   Every 15*128=1920 audio samples at 96 kHz produce 128 samples at 6.4 kHz
 */

 // NOTE Changed class name to start with capital "R"  RSL 7 Nov 2022

// *******************   TEENSY 4.x  ONLY   *******************
#if defined(__IMXRT1062__)

#include <Arduino.h>
#include "radioFT8Demodulator_F32.h"

// decimate15() is common code for several cases of decimation.
// Input is 128 word of data, in outFIR1,
// ready to be decimated by 15 to FT8 6.4 kHz sample rate.
void RadioFT8Demodulator_F32::decimate15(void)  {
   // Stage 1, decimate by 5.
   //    current128Used1 = true  means that outFIR1[] is empty
   //    current128Used1 = false means that outFIR1[] has some content
   // For 48 kHz sample rate, here twice every 2.6667 milliseconds
   while(!current128Used1)  // While we still have filtered input data
      {
      // Every time here dec1Count grows by 51.2, i.e., by 2*128/5 mod 128
      inFIR2[dec1Count++] = outFIR1[kOffset1];  // The 5th sample
      kOffset1 += 5;     // Update offset
      if(kOffset1>127)   // We have run out of source data for now
         {
         kOffset1 -= 128; // For the next 128
         current128Used1 = true;
         }
      if(dec1Count >= 128)    // Time to do second FIR filter
         {
         // Here twice every 13.3 millisec, 2*2.66667*2.5=13.3333
         arm_fir_f32(&fir_inst2, inFIR2, outFIR2, 128);
         current128Used2 = false;  // Just filled outFIR2[]
         dec1Count = 0;
         // Have 128 samples from the first divide by 5 stage, filtered
         // and ready for divide by 3.
         //    current128Used2 = true  means that outFIR2[] is empty
         //    current128Used2 = false means that outFIR2[] has some content
         while(!current128Used2)   // Over all outputs of FIR2
            {
            outData[dec2Count++] = outFIR2[kOffset2];  // Every 3rd sample

            kOffset2 += 3;     // Update offset for decimate by 3
            if(kOffset2 > 127)   // We have run out of stage 2 data
               {
               kOffset2 -= 128; // For the next 128
               current128Used2 = true;
               }

            if(dec2Count >= 128)    // 128 block is ready
               {
               // Arrive here about every 20 millisec (twice per 40.000)  18.7, 21.3
               dec2Count = 0;

               // Bonus power calculation.  Takes about 5 uSec
               powerOut = 0.0f;
               for(int kk=0; kk<128; kk++)
                  {
                  powerOut += (outData[kk]*outData[kk]);          // Power
                  }
               powerOut = 10.0f*log10f(powerOut) - 21.072f;   // in dB, corrected for 128
               powAvail = true;

#ifdef W5BAA_INTERFACE
               block = allocate();  // Get 128 int16 block for queue out
               if (!block)
                  return;

               h = head + 1;
               if (h >= max_buffers)
               h = 0;
               if (h == tail)    // Ooops, ran out of queue memory blocks
                  AudioStream::release(block);
               else  // Fill the block data
                  {
                  for(int kk=0; kk<128; kk++)
                     {
                     block->data[kk] = (int)(32768.0f*outData[kk]);   // Output signal
                     }
                  queue[h] = block;
                  head = h;
                  //   current128Used2 = false;
                  }
#else
   // Not W5BAA interface, i.e., 512 floats at a time.

   // To allow correction for time and frequency, the FT8 synch system performs
   // FFT every 80 mSec with frequency steps of 3.125 Hz.  80 mSec at 6400
   // samples/sec is 512 samples.  Thus data is sent from this class after
   // 512 samples.  The data set is 2048 floats.

   // At 6400 decimated samples/sec, it takes 128/6400=0.02 sec to fill
   // outData[128].  The data2k[2048] will be re-used after that time
   // which should be plenty adequate for the FFT to be done.  Be sure that
   // that the FFT is not delayed a large amount.

               // FFT is 2048, but we need overlap every 512 points.
               // Next is the 2048 FFT input data buffer. Shift 512
               // and add in the Temp 512 data pts.
               if(block128Count==0)
                  {
                  for (int kk=0; kk<1536; kk++)
                     {
                     data2K[kk] = data2K[kk + 512];  // Shift 512
                     }
                  }
               for(int kk=0; kk<128; kk++)
                  data2K[1536 + 128*block128Count + kk] = outData[kk];

               if(block128Count++ >= 3)    // Time to send data for FFT
                  {
                  block128Count = 0;
                  index2 = 0;
                  outputFlag = true;

                  FFTCount++;     // Count of incoming samples in units of 512
                  if(FFTCount >= 184)
                     {
                     dec1Count = 0;
                     dec2Count = 0;
                     index2 = 0;
                     //FFTCount = 0;    // Reset by startDataCollect(void)
                     gettingData = false;
                     }
                  }    // End, time to make 512 data pts available
#endif

               }  // End if(dec2Count >= 128)
            }     // End,  inner while(), i.e., while(!current128Used2)
         }
      }           // End, while have input data
   }

// Note: Suppports block size of 128 only.  Very "built in."
void RadioFT8Demodulator_F32::update(void)  {
   audio_block_f32_t *block_in;

   if(!gettingData)
      return;

   block_in = receiveReadOnly_f32(0);
   if (srIndex==SR_NONE || !block_in) { Serial.println("Block error"); return; }
// ttt=micros();
   // Here every 2.6667 millisec for 48 kHz, 128 pts
   current128Used1 = false;   // There are 128 new input data to use
   if(srIndex == SR48000)
      {
      for(int k=0; k<64; k++) // Over half the current block of samples
         {
         // Make next 2 same. Linearty of Fourier transform says
         // the result is the same either transform x2 in magnitude.
         dataIn[2*k] =   0.5f*block_in->data[k];
         dataIn[2*k+1] = 0.5f*block_in->data[k];
         }
      arm_fir_f32(&fir_inst1, dataIn, outFIR1, 128); // LP filter <9600 Hz
      current128Used1 = false;  // Just created 128 in outFIR1
      decimate15();
      for(int k=64; k<128; k++) // Ditto over the second half of samples
         {
         dataIn[2*k-128] = 0.5f*block_in->data[k];
         dataIn[2*k-127] = 0.5f*block_in->data[k];
         }
      arm_fir_f32(&fir_inst1, dataIn, outFIR1, 128); // LP filter <9600 Hz
      current128Used1 = false;  // Just created a second 128, outFIR1
      decimate15();
      }
   else  // SR_INDEX == SR96000
      {
      arm_fir_f32(&fir_inst1, block_in->data, outFIR1, 128); // LP filter
      decimate15();
      }
   AudioStream_F32::release(block_in);
#ifndef W5BAA_INTERFACE
   if(FFTCount>FFTOld)
      { FFTOld=FFTCount; }
#endif
// Serial.print("time usec              ");  Serial.println(micros()-ttt);
   }  // End update()
#endif
