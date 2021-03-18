/* synth_sin_cos_f32.cpp
 * 
 * SynthSinCos_F32  Bob Larkin April 17, 2020
 *
 * Based on Chip Audette's OpenAudio sine(), that was
 * Modeled on: AudioSynthWaveformSine from Teensy Audio Library
 *
 * Purpose: Create sine and cosine wave of given amplitude, frequency
 * and phase.  Outputs in float32_t floating point.
 * Routines are from the arm CMSIS library and use a 512 point lookup
 * table with linear interpolation to achieve float accuracy limits.
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
 */

// Rev 10 March 2021 - Corrected interpolation formula  Bob L

#include "synth_sin_cos_f32.h"

// 513 values of the sine wave in a float array:
#include "sinTable512_f32.h"

void AudioSynthSineCosine_F32::update(void) {
    audio_block_f32_t *blockS, *blockC;
    uint16_t index, i;
    float32_t a, b, deltaPhase, phaseC;
    blockS = AudioStream_F32::allocate_f32();   // Output blocks
    if (!blockS)  return;
    
    blockC = AudioStream_F32::allocate_f32();
	if (!blockC) {
		AudioStream_F32::release(blockS);
		return;
    }
 
    // doSimple has amplitude (-1, 1) and sin/cos differ by 90.00 degrees.
    if (doSimple) {
       for (i=0; i < block_length; i++) {
          phaseS += phaseIncrement;
          if (phaseS > 512.0f)
            phaseS -= 512.0f;
          index = (uint16_t) phaseS;
          deltaPhase = phaseS -(float32_t) index;
          /* Read two nearest values of input value from the sin table */
          a = sinTable512_f32[index];
          b = sinTable512_f32[index+1];
          blockS->data[i] = a + 0.001953125*(b-a)*deltaPhase;  /* Linear interpolation process */
 
          /* Repeat for cosine by adding 90 degrees phase  */
          index = (index + 128) & 0x01ff;
          /* Read two nearest values of input value from the sin table */
          a = sinTable512_f32[index];
          b = sinTable512_f32[index+1];
          /* deltaPhase will be the same as used for sin  */
          blockC->data[i] = a + 0.001953125*(b-a)*deltaPhase;  /* Linear interpolation process */
       }
    }
    else {   // Do a more flexible update, i.e., not doSimple
       for (i=0; i < block_length; i++) {
          phaseS += phaseIncrement;
          if (phaseS > 512.0f)  phaseS -= 512.0f;
          index = (uint16_t) phaseS;
          deltaPhase = phaseS -(float32_t) index;
          /* Read two nearest values of input value from the sin table */
          a = sinTable512_f32[index];
          b = sinTable512_f32[index+1];
          blockS->data[i] = amplitude_pk*(a + 0.001953125*(b-a)*deltaPhase);  /* Linear interpolation process */
 
          /* Shift forward phaseS_C  and get cos. First, the calculation of index of the table */
          phaseC = phaseS + phaseS_C;
          if (phaseC > 512.0f)  phaseC -= 512.0f;
          index = (uint16_t) phaseC;
          deltaPhase = phaseC -(float32_t) index;
          /* Read two nearest values of input value from the sin table */
          a = sinTable512_f32[index];
          b = sinTable512_f32[index+1];
          blockC->data[i] = amplitude_pk*(a + 0.001953125*(b-a)*deltaPhase);  /* Linear interpolation process */
        }
    }
    AudioStream_F32::transmit(blockS, 0);
    AudioStream_F32::release (blockS);       
    AudioStream_F32::transmit(blockC, 1);
    AudioStream_F32::release (blockC);
    return;
}

