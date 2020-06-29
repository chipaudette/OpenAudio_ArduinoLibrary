/*
 * AudioAnalyzePhase_F32.cpp
 *
 * 31 March 2020  Rev 8 April 2020
 * Bob Larkin, in support of the library:
 * Chip Audette, OpenAudio, Apr 2017
 *
 *  * Copyright (c) 2020 Bob Larkin
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
 */
/* There are two inputs, I and Q (Left and Right  or 0 and 1)
 * There is one output, the phase angle between the two inputs expressed in
 * radians (180 degrees is Pi radians).  See AudioAnalyzePhase_F32.h
 * for details.
 */

#include "AudioAnalyzePhase_F32.h"
#include <math.h>
#include "mathDSP_F32.h"

void AudioAnalyzePhase_F32::update(void) {
  audio_block_f32_t *block0, *block1;
  uint16_t i;
  float32_t mult0, mult1, min0, max0, minmax0, min1, max1, minmax1;
  float32_t min_max = 1.0f;   // Filled in correctly, later 
  mathDSP_F32 mathDSP1;
  
#if TEST_TIME
  if (iitt++ >1000000) iitt = -10;
  uint32_t t1, t2; 
  t1 = tElapse;
#endif

  // Get first input, 0
  block0 = AudioStream_F32::receiveWritable_f32(0);
  if (!block0)  {
     if(errorPrint)  Serial.println("AN_PHASE ERR: No input memory");
     return;
  }

  // Get second input, 1
  block1 = AudioStream_F32::receiveWritable_f32(1);
  if (!block1){
     if(errorPrint)  Serial.println("AN_PHASE ERR: No input memory");
     AudioStream_F32::release(block0);
   return;
  }
	
   // Limiter on both inputs; can be used with narrow IIR filter
   if (pdConfig & LIMITER_MASK) {
      for (uint16_t j = 0; j<block0->length; j++) {
          if (block0->data[j]>=0.0f)
             block0->data[j] = 1.0f;
          else
             block0->data[j] = -1.0f;

          if (block1->data[j]>=0.0f)
             block1->data[j] = 1.0f;
          else
             block1->data[j] = -1.0f;
       }
   }

  // If needed, find a scaling factor for the multiplier type phase det
  // This would not normally be used with a limiter---they are
  // both trying to solve the same problem.
  if(pdConfig & SCALE_MASK) {
       min0 = 1.0E6f;   max0 = -1.0E6f;
       min1 = 1.0E6f;   max1 = -1.0E6f;
       for (i=0; i < block0->length; i++){
           if(block0->data[i] < min0) min0 = block0->data[i];
           if(block0->data[i] > max0) max0 = block0->data[i];
           if(block1->data[i] < min1) min1 = block1->data[i];
           if(block1->data[i] > max1) max1 = block1->data[i];
        }
        minmax0 = max0 - min0;   // 0 to 2
        minmax1 = max1 - min1;   // 0 to 2
        min_max = minmax0 * minmax1;  // 0 to 4  
   }

   // multiply the two inputs to get phase plus second harmonic.  Low pass
   // filter  and then apply ArcCos of the result to return the phase difference
   // in radians.  Multiply and leave result in 1
   arm_mult_f32(block0->data, block1->data, block1->data, block0->length);

   if (LPType == NO_LP_FILTER) {
       for (i=0; i < block0->length; i++)
           block0->data[i] = block1->data[i];   // Move back to block0
   }
   else if(LPType == IIR_LP_FILTER) {
       // Now filter 1, leaving result in 0.  This is 4 BiQuads
       arm_biquad_cascade_df1_f32(&iir_inst, block1->data, block0->data, block0->length);
   }
   else {      // Alternate FIR filter for FIR_LP_FILTER
       arm_fir_f32(&fir_inst, block1->data, block0->data, block0->length);
   }
   AudioStream_F32::release(block1);     // Not needed further

   /* For variable, pdConfig:
    * LIMITER_MASK: 0=No Limiter   1=Use limiter
    * ACOS_MASK:    00=Use no acos linearizer    01=undefined
    *               10=Fast, math-continuous acos() (default)    11=Accurate acos()
    * SCALE_MASK:   0=No scale of multiplier  1=scale to min-max (default)
    * UNITS_MASK:   0=Output in degrees    1=Output in radians (default)
    */
   if(pdConfig & SCALE_MASK)
       mult0 =  8.0f / min_max;
   else
       mult0 = 2.0f;

   if(pdConfig & UNITS_MASK)
       mult1 = 1.0;
   else
       mult1 = 57.295779f;

   // Optionally, apply ArcCos() to linearize block0 output in radians * units to scale to degrees
   if (pdConfig & ACOS_MASK) {
       if((pdConfig & ACOS_MASK) == 0b00110) {     // Accurate acosf from C-library
           for (uint i = 0; i<block_size; i++) {
			   block0->data[i] =  mult1 * acosf(mult0 * block0->data[i]); 
		   }
	   }
       else if ((pdConfig & ACOS_MASK) == 0b00100) {  // Fast acos from polynomial
		   for (uint i = 0; i<block_size; i++) {
               block0->data[i] =  mult1 * mathDSP1.acos_f32(mult0 * block0->data[i]);
           }
       }
    }
    // Not applying linearization or scaling, at all.  Works for multiplier outputs near 0 (90 deg difference)
    else {
        float32_t mult = 8.0f * mult0 * mult1;
        for (uint i = 0; i<block_size; i++) {
           block0->data[i] = mult * block0->data[i];
        }
    } 
    AudioStream_F32::transmit(block0, 0);
    AudioStream_F32::release(block0);
 #if TEST_TIME    
  t2 = tElapse;
  if(iitt++ < 0) {Serial.print("At AnalyzePhase end, microseconds = ");  Serial.println (t2 - t1); }  
  t1 = tElapse;
 #endif   
}   // End update()
