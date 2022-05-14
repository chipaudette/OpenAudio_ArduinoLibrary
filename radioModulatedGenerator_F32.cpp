/*     radioModulatedGenerator_F32.cpp
 *
 * RadioModulatedGenerator_F32 class - See .h file for information.
 * Copyright (c) 2021 Bob Larkin            Created: 15 April 2021
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

#include "radioModulatedGenerator_F32.h"

// 513 values of the sine wave in a float array:
#include "sinTable512_f32.h"

void radioModulatedGenerator_F32::update(void) {
  audio_block_f32_t *inAmpl,  *inPhaseFreq;
  audio_block_f32_t *outBlockI, *outBlockQ;
  uint16_t index, i;
  float32_t a, b, deltaPhase, phaseC, amSig;

  // Input 0 is for amplitude modulation.
  inAmpl = NULL;
  if(doAM) {
    inAmpl = AudioStream_F32::receiveReadOnly_f32(0);
    if (!inAmpl)  return;
    }

  // Input 1 is for phase or frequency modulation.
  inPhaseFreq = NULL;
  if(doPM || doFM)  {
    inPhaseFreq = AudioStream_F32::receiveReadOnly_f32(1);
    if (!inPhaseFreq) {
       if(doAM) AudioStream_F32::release(inAmpl);
       return;
    }
  }

  outBlockI = AudioStream_F32::allocate_f32();   // Output blocks
  if (!outBlockI)  {
     if(doAM) AudioStream_F32::release(inAmpl);
     if(doPM || doFM) AudioStream_F32::release(inPhaseFreq);
     return;
  }

  outBlockQ = NULL;
  if(bothIQ)  {
     outBlockQ = AudioStream_F32::allocate_f32();
     if (!outBlockQ) {
        if(doAM) AudioStream_F32::release(inAmpl);
        if(doPM || doFM) AudioStream_F32::release(inPhaseFreq);
        AudioStream_F32::release(outBlockI);
        return;
     }
  }

  for (i=0; i < block_length; i++) {
     if(doPM)  // Phase in inPhaseFreq->data[i] is scaled for (0.0, 2*PI)
        phaseS += (phaseIncrement0 + K512ON2PI*inPhaseFreq->data[i]);
     else if(doFM)
        phaseS += kp*(freq + deviationFMScale*inPhaseFreq->data[i]);  //  kp=512.0/sample_rate_Hz
     else
        phaseS += phaseIncrement0;  // No PM or FM alteration to carrier phase

     while (phaseS > 512.0f)
        phaseS -= 512.0f;
     while (phaseS < 0.0f)
        phaseS += 512.0f;
     index = (uint16_t) phaseS;    // Does adding 0.5 here cut errors?  <<<<<<<<<<<<<<<<<<
     deltaPhase = phaseS -(float32_t) index;
     /* Read two nearest values of input value from the sin table */
     a = sinTable512_f32[index];
     b = sinTable512_f32[index+1];
     if(doAM) {
        amSig = 1.0f + inAmpl->data[i];
        if(amSig<0.0f)
           amSig = 0.0f;        // Common def of AM going back to vacuum tubes
        outBlockI->data[i] = amplitude_pk*amSig*(a + 0.001953125*(b-a)*deltaPhase);  /* Linear interpolation process */
     }
     else
        outBlockI->data[i] = amplitude_pk*(a + 0.001953125*(b-a)*deltaPhase);

     if(bothIQ)  {
        /* Shift forward phaseQ_I  and get cos. First, the calculation of index of the table */
        phaseC = phaseS + phaseQ_I;
        while (phaseC > 512.0f)
           phaseC -= 512.0f;
        while (phaseC < 0.0f)
           phaseC += 512.0f;
        index = (uint16_t) phaseC;
        deltaPhase = phaseC -(float32_t) index;
        /* Read two nearest values of input value from the sin table */
        a = sinTable512_f32[index];
        b = sinTable512_f32[index+1];
        if(doAM)                  // amSig from above
          outBlockQ->data[i] = amplitudeQ_I*amplitude_pk*amSig*(a + 0.001953125*(b-a)*deltaPhase);
        else
          outBlockQ->data[i] = amplitudeQ_I*amplitude_pk*(a + 0.001953125*(b-a)*deltaPhase);
      }
   }
   if(doAM)         AudioStream_F32::release(inAmpl);
   if(doPM || doFM) AudioStream_F32::release(inPhaseFreq);
   AudioStream_F32::transmit(outBlockI, 0);
   AudioStream_F32::release (outBlockI);
   if(bothIQ)  {
      AudioStream_F32::transmit(outBlockQ, 1);
      AudioStream_F32::release (outBlockQ);
   }
}
