/*
 * AudioFilter90Deg_F32.h
 * 22 March 2020     Bob Larkin
 * Parts are based on Open Audio FIR filter by Chip Audette:
 * 
 * Chip Audette (OpenAudio) Feb 2017
 *    - Building from AudioFilterFIR from Teensy Audio Library
 *     (AudioFilterFIR credited to Pete (El Supremo))
 * Copyright (c) 2020 Bob Larkin
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
 
/*
 * This consists of two uncoupled paths that almost have the same amplitude gain
 * but differ in phase by exactly 90 degrees.   See AudioFilter90Deg_F32.cpp
 * The number of coefficients is an odd number for the FIR Hilbert transform
 * as that produces an easily achievable integer sample period delay.  In
 * float, the ARM FIR library routine will handle odd numbers.\, so no zero padding
 * is needed.
 * 
 * No default Hilbert Transform is provided, as it is highly application dependent.
 * The number of coefficients is an odd number with a maximum of 250.  The Iowa
 * Hills program can design a Hilbert Transform filter. Use begin(*pCoeff, nCoeff) 
 * in the .INO to initialize this block.
 *
 * Status: Tested T3.6 and T4.0.  No known bugs.
 * Functions:
 *   begin(*pCoeff, nCoeff);  Initializes this block, with:
 *         pCoeff = pointer to array of F32 Hilbert Transform coefficients
 *         nCoeff = uint16_t number of Hilbert transform coefficients
 *   showError(e);  Turns error printing in update() on (e=1) and off (e=0). For debug.
 * Examples:    
 *   ReceiverPart1.ino
 *   ReceiverPart2.ino
 * Time: Depends on size of Hilbert FIR. Time for main body of update() including
 *   Hilbert FIR and compensating delay, 128 data block, running on Teensy 3.6 is:
 *     19 tap Hilbert (including 0's)  74 microseconds
 *    121 tap Hilbert (including 0's) 324 microseconds
 *    251 tap Hilbert (including 0's) 646 microseconds
 *   Same 121 tap Hilbert on T4.0 is 57 microseconds per update()
 *   Same 251 tap Hilbert on T4.0 is 114 microseconds per update()
 */

#ifndef _filter_90deg_f32_h
#define _filter_90deg_f32_h

#include "AudioStream_F32.h"
#include "arm_math.h"

#define TEST_TIME_90D 1

// Following supports a maximum FIR Hilbert Transform of 251
#define HILBERT_MAX_COEFFS 251

class AudioFilter90Deg_F32 : public AudioStream_F32 {
//GUI: inputs:2, outputs:2  //this line used for automatic generation of GUI node
//GUI: shortName: 90DegPhase
public:
    // Option of AudioSettings_F32 change to block size (no sample rate dependent variables here):
    AudioFilter90Deg_F32(void) :  AudioStream_F32(2, inputQueueArray_f32) {
        block_size = AUDIO_BLOCK_SAMPLES;
    }
    AudioFilter90Deg_F32(const AudioSettings_F32 &settings) :  AudioStream_F32(2, inputQueueArray_f32) {
        block_size = settings.audio_block_samples;
    }

    // Initialize the 90Deg by giving it the filter coefficients and number of coefficients
    // Then the delay line for the q (Right) channel is initialized
    void begin(const float32_t *cp, const int _n_coeffs) {
        coeff_p = cp;
        n_coeffs = _n_coeffs;

        // Initialize FIR instance (ARM DSP Math Library)  (for f32 the return is always void)
        if (coeff_p!=NULL  && n_coeffs<252) {
            arm_fir_init_f32(&Ph90Deg_inst, n_coeffs, (float32_t *)coeff_p,  &StateF32[0], block_size);
        }
        else {
            coeff_p = NULL;     // Stops further FIR filtering for Hilbert
            // Serial.println("Hilbert: Missing FIR Coefficients or number > 251");
        }

        // For the equalizing delay in q, if n_coeffs==19, n_delay=9
        // Max of 251 coeffs needs a delay of 125 sample periods.
        n_delay = (uint8_t)((n_coeffs-1)/2);
        in_index = n_delay;
        out_index = 0;
        for (uint16_t i=0;  i<256; i++){
            delayData[i] = 0.0F;
        }
    }       // End of begin()

    void showError(uint16_t e) {
        errorPrint = e;
    }

    void update(void);

private:
    uint16_t block_size =  AUDIO_BLOCK_SAMPLES;
    // Two input data pointers
    audio_block_f32_t *inputQueueArray_f32[2];
    // One output pointer
    audio_block_f32_t *blockOut_i;

#if TEST_TIME_90D
//  *Temporary* - allows measuring time in microseconds for each part of the update()
elapsedMicros tElapse;
int32_t iitt = 999000;     // count up to a million during startup
#endif

    // Control error printing in update() 0=No print
    uint16_t errorPrint = 0;

    //float32_t tmpHil[5]={0.0, 1.0, 0.0, -1.0, 0.0};   coeff_p = &tmpHil[0];
    // pointer to current coefficients or NULL
    const float32_t *coeff_p = NULL;
    uint16_t n_coeffs = 0;

    // Variables for the delayed q-channel:
    // For the q-channel, we need a delay of ((Ncoeff - 1) / 2)  samples.  This
    // is 9 delay for 19 coefficient FIR.  This can be implemented as a simple circular
    // buffer if we make the buffer a power of 2 in length and binary-truncate the index.
    // Choose 2^8 = 256.  For a 251 long Hilbert this wastes 256-128-125 = 3, but
    // more for shorter Hilberts.
    float32_t delayData[256];   // The circular delay line
    uint16_t in_index;
    uint16_t out_index;
    // And a mask to make the circular buffer limit to a power of 2
    uint16_t delayBufferMask = 0X00FF;
    uint16_t n_delay;

    // ARM DSP Math library filter instance
    arm_fir_instance_f32 Ph90Deg_inst;
    float32_t StateF32[AUDIO_BLOCK_SAMPLES + HILBERT_MAX_COEFFS];
};
#endif
