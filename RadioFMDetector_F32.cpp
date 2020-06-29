/*
 * RadioFMDetector_F32.cpp
 *
 * 22 March 2020
 * Bob Larkin, in support of the library:
 * Chip Audette, OpenAudio, Apr 2017
 *     -------------------
 *
 * Copyright (c) 2020 Bob Larkin
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
 * See RadioFMDetector_F32.h for usage details
*/
#include "RadioFMDetector_F32.h"

// 513 values of the sine wave in a float array:
#include "sinTable512_f32.h"

//  ====  UPDATE  ====
void RadioFMDetector_F32::update(void) {
  audio_block_f32_t  *blockIn, *blockOut=NULL;
  
  uint16_t i, index_sine;
  float32_t deltaPhase, a, b, dtemp1, dtemp2;
  float32_t  v_i[128];  // max size
  float32_t  v_q[128];
  mathDSP_F32 mathDSP1;    // Math support functions
  
#if TEST_TIME_FM
  if (iitt++ >1000000) iitt = -10;
  uint32_t t1, t2; 
  t1 = tElapse;
#endif

  // Get input to FM Detector block
  blockIn = AudioStream_F32::receiveWritable_f32(0);
  if (!blockIn) {
     if(errorPrintFM)  Serial.println("FMDET-ERR: No input memory");
     return;
   }

  // If there's no coefficient table, give up.
  if (fir_IQ_Coeffs == NULL) {
    if(errorPrintFM)  Serial.println("FMDET-ERR: No IQ FIR Coefficients");
    AudioStream_F32::release(blockIn);
    return;
  }

  if (fir_Out_Coeffs == NULL) {
    if(errorPrintFM)  Serial.println("FMDET-ERR: No Out FIR Coefficients");
    AudioStream_F32::release(blockIn);
    return;
  }

  // Try to get a block for the FM output
  blockOut = AudioStream_F32::allocate_f32();
  if (!blockOut){      // Didn't have any
    if(errorPrintFM)  Serial.println("FMDET-ERR: No Output Memory");
    AudioStream_F32::release(blockIn);
    return;
  }

  // Generate sine and cosine of center frequency and double-balance mix
  // these with the input signal to produce an intermediate result
  // saved as v_i[] and v_q[]
  for (i=0; i < block_size; i++) {
      phaseS += phaseIncrement;
      if (phaseS > 512.0f)
         phaseS -= 512.0f;
      index_sine = (uint16_t) phaseS;
      deltaPhase = phaseS -(float32_t) index_sine;
      /* Read two nearest values of input value from the sin table */
      a = sinTable512_f32[index_sine];
      b = sinTable512_f32[index_sine+1];
      // Linear interpolation and multiplying (DBMixer) with input
      v_i[i] = blockIn->data[i] * (a + 0.001953125*(b-a)*deltaPhase);

      /* Repeat for cosine by adding 90 degrees phase  */
      index_sine = (index_sine + 128) & 0x01ff;
      /* Read two nearest values of input value from the sin table */
      a = sinTable512_f32[index_sine];
      b = sinTable512_f32[index_sine+1];
      /* deltaPhase will be the same as used for sin  */
      v_q[i] = blockIn->data[i] * (a + 0.001953125*(b-a)*deltaPhase);
   }
 
   // Do I FIR and Q FIR. We can borrow blockIn and blockOut at this point
   //void arm_fir_f32( const arm_fir_instance_f32* S, float32_t* pSrc, float32_t* pDst, uint32_t blockSize)
   arm_fir_f32(&FMDet_I_inst, v_i, blockIn->data,  (uint32_t)blockIn->length);
   arm_fir_f32(&FMDet_Q_inst, v_q, blockOut->data, (uint32_t)blockOut->length);
   // Do ATAN2, differentiation and de-emphasis  in single loop
   for(i=0; i<block_size; i++) { //  y        x
       dtemp1 = mathDSP1.fastAtan2((float)blockOut->data[i], (float)blockIn->data[i]);
       // Apply differentiator by subtracting last value of atan2
       if(dtemp1>MF_PI_2  &&  diffLast<-MF_PI_2)       // Probably a wrap around
           dtemp2 = dtemp1 - diffLast - MF_TWOPI;
       else if(dtemp1<-MF_PI_2  &&  diffLast >MF_PI_2)  // Probably a reverse wrap around
           dtemp2 = dtemp1 - diffLast + MF_TWOPI;
       else
           dtemp2 = dtemp1 - diffLast;      // Differentiate
       diffLast = dtemp1;               // Ready for next time through loop
       // Data point is now dtemp2. Apply single pole de-emphasis LPF, in place
       dLast = Kdem * dtemp2 + OneMinusKdem * dLast;
       blockIn->data[i] = dLast;        // and save to an array
    }

    // Do output FIR filter.  Data now in blockIn.
    arm_fir_f32(&FMDet_Out_inst, blockIn->data, blockOut->data,  (uint32_t)blockIn->length);
    AudioStream_F32::release(blockIn);

    // Transmit the data
    AudioStream_F32::transmit(blockOut, 0);
    AudioStream_F32::release(blockOut);
#if TEST_TIME_FM    
  t2 = tElapse;
  if(iitt++ < 0) {Serial.print("At end of FM Det ");  Serial.println (t2 - t1); }  
  t1 = tElapse;
#endif   
}
