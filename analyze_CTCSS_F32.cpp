/*
 * analyze_CTCSS_F32.cpp  Converted to float from PJRC Teensy Audio Library
 *   MIT License on changed portions
 *   Bob Larkin March 2021
 *   Rev 15 March 2023 - Added dynamic sample rate control; added 12 ksps; corrected
 *      receiveWritable_f32() for block with output.  Bob L - see .h file.
 *
 * Some parts of this  cpp came from:  Audio Library for Teensy 3.X
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
/* From Pavel Rajmic (2021). Generalized Goertzel algorithm
 * https://www.mathworks.com/matlabcentral/fileexchange/35103-generalized-goertzel-algorithm
 * MATLAB Central File Exchange. Retrieved December 18, 2021.
 */

#include <Arduino.h>
#include "analyze_CTCSS_F32.h"

void analyze_CTCSS_F32::update(void)  {
    audio_block_f32_t *block;
    float32_t gs0=0.0;
    block = AudioStream_F32::receiveWritable_f32();
    if (!block) return;
    if (!gEnabled) {
        release(block);
        return;
        }

    // This process is working with frequencies of 254 Hz or less.  Thus
    // it is beneficial to decimate before processing.  The decimation ratio
    // is 16 or 8. For example, if the basic sample rate is 44.1 kHz,
    // the decimated rate is 44100/16=2756.25 Hz  Before decimation, we
    // low pass filter to prevent alias problems.  Returns 128 pts in block.
    arm_biquad_cascade_df1_f32(&iir_lpf_inst, block->data,
           block->data, block->length);

    // And decimate, using the first sample, giving 128/16=8 samples to
    // be processed per block.
    // Create a small data block, normally 8, d16a[].
    uint16_t nPerBlock2 = 128/nDecimate;
    for(int i=0; i<nPerBlock2; i++)
       d16a[i] = *(block->data + nDecimate*i);   // Decimated sample, only every nDecimate

    // Filter down to 67-254Hz band, leaving result in d16a[];
    arm_biquad_cascade_df1_f32(&iir_bpf_inst, d16a, d16a, nPerBlock2);

    // For reference measurement, only, null out the tone, creating d16b[].
    // This d16b signal  path is not used for the Goertzel.
    arm_biquad_cascade_df1_f32(&iir_nulls_inst, d16a, d16b, nPerBlock2);

    for(int i=0; i<nPerBlock2; i++)
       {
       if(gCount++ < gLength)            // Main Goertzel calculation
          {
		  // Use decimated sample, only every 16
          gs0 = d16a[i] + gCoefficient*gs1 - gs2;
          gs2 = gs1;
          gs1 = gs0;
          // Do reference channel power measurement of the 67 to 254 Hz
          // band, including nulls at tone frequency.
          powerSum += d16b[i]*d16b[i];
          powerSumCount++;
          }
        else                             // The last, special, calculation
          {
          gs0 = gCoefficient*gs1 - gs2; // Generalized non-integer freq Goertzel
          out1 = gs0 - gs1*ccRe;
          out2 =     - gs1*ccIm;
          // At this point the phase is still in need of correction.  But we
          // only use amplitude so, leave the phase as is.
          powerTone = 4.0f*(out1*out1 + out2*out2)/((float32_t)gLength*(float32_t)gLength);
          gs1 = 0.0;        // Initialize to start new measurement
          gs2 = 0.0;
          gCount = 0;
          // The 2.04 factor makes the reference for sine waves the same as the Goertzel
          powerRef = 2.04f*powerSum/((float32_t)powerSumCount);
          powerSum = 0.0f;
          powerSumCount = 0;
          new_output = true;
          }
        }

    // If the CTCSS tone is detected, the output is the original data block.
    // If silenced, zeros are returned.
    if(powerTone>threshAbs  &&  powerTone>threshRel*powerRef)
       transmit(block);
    else
       {
	    for(int i = 0; i<128; i++)
	        *(block->data + i) = 0.0f;
	   transmit(block);
       }
    release(block);
    }

analyze_CTCSS_F32::operator bool()  {
    float32_t pThresh;
    pThresh = ((float32_t)gLength)*threshAbs;
    pThresh *= pThresh;
    return (powerTone >= pThresh);
    }
