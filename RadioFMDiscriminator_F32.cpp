/*
 * RadioFMDetector_F32.cpp
 *
 * 25 April 2022
 * Bob Larkin, in support of the library:
 * Chip Audette, OpenAudio, Apr 2017
 *     -------------------
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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * See RadioFMDiscriminator_F32.h for usage details
*/
#include "RadioFMDiscriminator_F32.h"

//  ====  UPDATE  ====
void RadioFMDiscriminator_F32::update(void) {
   audio_block_f32_t  *blockIn, *blockA=NULL, *blockB=NULL;
   int i;
   static float32_t discr0, disc1, disc2;
   static float32_t sumNoise;
   static float saveIn = 0.0f;  // for squelch
   static float saveOut = 0.0f;

#if TEST_TIME_FM
   if (iitt++ >1000000) iitt = -10;
   uint32_t t1, t2;
   t1 = tElapse;
#endif

   // Get input to FM Discriminator block
   blockIn = AudioStream_F32::receiveWritable_f32(0);
   if (!blockIn) {
      return;
      }

   if (fir_Out_Coeffs == NULL) {
      //if(errorPrintFM)  Serial.println("FMDET-ERR: No Out FIR Coefficients");
      AudioStream_F32::release(blockIn);
      return;
      }

   // Try to get two blocks for both intermediate data and outputs
   blockA = AudioStream_F32::allocate_f32();
   if (!blockA){      // Didn't have any
      //if(errorPrintFM)  Serial.println("FMDET-ERR: No Output Memory");
      AudioStream_F32::release(blockIn);
      return;
      }

   blockB = AudioStream_F32::allocate_f32();
   if (!blockB)     // Didn't have any
      {
      //if(errorPrintFM)  Serial.println("FMDET-ERR: No Output Memory");
      AudioStream_F32::release(blockIn);
      AudioStream_F32::release(blockA);
      return;
      }

   // Limiter
   for(int k=0; k<128; k++)
      blockIn->data[k] = (blockIn->data[k]>0.0f) ? 1.0f : -1.0f;

   // Two BPF  at f1 and f2
   arm_biquad_cascade_df1_f32(&f1BPF_inst, blockIn->data,
        blockA->data, blockIn->length);
   arm_biquad_cascade_df1_f32(&f2BPF_inst, blockIn->data,
        blockB->data, blockIn->length);

   if ( isnan(discrOut) ) discrOut=0.0f;

   // Find difference in responses and average
   for (i=0; i < block_size; i++)
      {
      // Find maximum absolute amplitudes (full-wave rectifiers)
      disc1 = blockA->data[i];
      disc1 = disc1>0.0 ? disc1 : -disc1;
      disc2 = blockB->data[i];
      disc2 = disc2>0.0 ? disc2 : -disc2;
      discr0 = disc2 - disc1;    // Sample-by-sample discriminator output

      // Low pass filtering, RC type
      discrOut = 0.999f*discr0 + 0.001f*discrOut;   //  REMOVE?????

      // Data point is now discrOut. Apply single pole de-emphasis LPF, in place
      // Set kDem = 1 to stop filtering
      //    dLast = Kdem * discrOut + OneMinusKdem * dLast;   //<<<<<<<<FIX <<<<<<
      //    blockA->data[i] = dLast;        // and save to an array
      blockA->data[i] = discrOut;

      //Serial.print(disc1, 6); Serial.print(" dLow  dHigh "); Serial.println(disc2, 6);
      //Serial.println(discrOut, 6);
      }

   // Do output FIR filter.  Data now in blockA.  Filter out goes to blockB.
   if(outputFilterType == LPF_FIR)
      arm_fir_f32(&FMDet_Out_inst, blockA->data, blockB->data, (uint32_t)blockIn->length);
   else if(outputFilterType == LPF_IIR)
      arm_biquad_cascade_df1_f32(&outLPF_inst, blockA->data, blockB->data, blockIn->length);
   else
      for(int k=0; k<blockIn->length; k++)
         blockB->data[k] = blockA->data[k];

    // The un-squelch controlled audio for tone decoding, etc., always goes to output 0
    AudioStream_F32::transmit(blockB, 0);

    // Squelch picks the audio from before the output filter and does a 4-pole BiQuad BPF
    // blockA->data still has the data we need.  Borrow blockIn for squelch filter out.
    arm_biquad_cascade_df1_f32(&iirSqIn_inst, blockA->data, blockIn->data, blockIn->length);

    // Update the Squelch full-wave envelope detector and single pole LPF
    sumNoise = 0.0f;
    for(i=0; i<block_size; i++)
       sumNoise += fabsf(blockIn->data[i]);  // Ave of rectified noise
    squelchLevel = alpha*(sumNoise + saveIn) + gamma*saveOut;  // 1 pole
    saveIn = sumNoise;
    saveOut = squelchLevel;
    // Add hysteresis here  <<<<

   // Squelch gate sends audio to output 1 (right) if squelch is open
   if(squelchLevel > squelchThreshold)
      AudioStream_F32::transmit(blockB, 1);
   else
      {
      for(int kk=0; kk<128; kk++)
         blockA->data[kk] = 0.0f;
      AudioStream_F32::transmit(blockA, 1);
      }

   AudioStream_F32::release(blockA);        // Give back the blocks
   AudioStream_F32::release(blockB);
   AudioStream_F32::release(blockIn);
#if TEST_TIME_FM
  t2 = tElapse;
  if(iitt++ < 0) {Serial.print("At end of FM Det ");  Serial.println (t2 - t1); }
  t1 = tElapse;
#endif
}
