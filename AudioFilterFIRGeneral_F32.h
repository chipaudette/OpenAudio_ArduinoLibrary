/*
 * AudioFilterFIRGeneral_F32
 *
 * Created: Bob Larkin   W7PUA   20 May 2020
 *
 * Credit and thanks to PJRC, Paul Stoffregen and Teensy products for the audio
 * system and library that this is built upon as well as the float32
 * work of Chip Audette embodied in the OpenAudio_ArduinoLibrary. Many thanks
 * for the library structures and wonderful Teensy products.
 *
 * There are enough different FIR filter Audio blocks to need a summary.  Here goes:
 * 
 * AudioFilterFIR   (Teensy Audio Library by PJRC) handles 16-bit integer data,
 *     and a maximum of 200 FIR coefficients, even only. (taps). For Teensy Audio.
 * AudioFilterFIR_F32 (OpenAudio_ArduinoLibrary by Chip Audette) handles 32-bit floating point
 *     data and a maximum of 200 taps.  Can be used with a mix of Teensy Audio
 *     and 32-bit OpenAudio_Arduino blocks.
 * AudioFilterFIRGeneral_F32 (This block)   handles 32-bit floating point
 *     data and very large  number of taps, even or odd.  Can be used with a mix of Teensy Audio
 *     and 32-bit OpenAudio_Arduino blocks.  This includes the design of an
 *     arbitrary frequency response using Kaiser windows.
 * AudioFilterFIRGeneral_I16 Same as this block, but data is 16-bit integer and
 *     the number of taps must be even.
 * AudioFilterEqualizer_F32  handles 32-bit floating point data and 250 maximum taps
 *     even or odd.  Can be used with a mix of Teensy Audio
 *     and 32-bit OpenAudio_Arduino blocks.  This includes the design of an
 *     arbitrary frequency response using multiple flat response steps.  A Kaiser windows
 *     is used.
 * AudioFilterEqualizer_I16  handles 16-bit integer data and 250 maximum taps,
 *     even only.
 *
 * FIR filters suffer from needing considerable computation of the multiply-and-add
 * sort.  This limits the number of taps that can be used, but less so as time goes by.
 * In particular, the Teensy 4.x, if it *did nothing but*  FIR calculations, could
 * use about 6000 taps inmonaural, which is a huge number.  But, this also 
 * suggests that if the filtering task is an important function of a project,
 * using, say 2000 taps is practical.
 * 
 * FIR filters can be (and are here) implemented to have symmetrical coefficients.  This
 * results in constant delay at all frequencies (linear phase).  For some applications this can
 * be an important feature.  Sometimes it is suggested that the FIR should not be
 * used because of the latency it creates.  Note that if constant delay is needed, the FIR
 * implementation does this with minimum latency.
 * 
 * For this block, AudioFilterFIRGeneral_F32, memory storage for the FIR
 * coefficiients as well as working storage for the ARM FIR routine is provided
 * by the calling .INO.  This allows large FIR sizes without always tying up a
 * big memory block.
 * 
 * This block normally calculates the FIR coefficients using a Fourier transform
 * of the desired amplitude response and a Kaiser window.  This flexability requires
 * the calling .INO to provide an array of response levels, in relative dB,
 * that is half the length of the number of FIR taps.  An example of this entry is a
 * 300 point low-pass filter with a cutoff at 10% of the 44.1 kHz sample frequency:
 *   for(int i=0; i<150; i++) {
 *     if (i<30)   dbA[i] = 0.0f;
 *     else        dbA[i] = -140.0f;
 *   }
 *   firg1.FIRGeneralNew(&dbA[0], 300, &equalizeCoeffs[0], 50.0f, &workSpace[0]);
 * 
 * As an alternate to inputting the response function, the FIR coefficients can be
 * entered directly using LoadCoeffs(nFIR, cf32f, *pStateArray).  This is a very quick
 * operation as only pointers to coefficients are involved.  Several filters can be
 * stored in arrays and switched quickly this way.  If this is done, pStateArray[]
 * as initially setup should be large enough for all filters. There will be "clicks"
 * associated with filter changes and these may need to be muted. 
 *
 * How well the desired response is achieved depends on the number of FIR coefficients
 * being used.  As noted above, for some applications it may be desired to use
 * large numbers of taps.  The achieved response can be evaluated 
 * by the function getResponse(nPoints, pResponse) which fills an .INO-supplied array
 * pResponse[nPoints] with the frequency response of the equalizer in dB.  The nPoints
 * are spread evenly between 0.0 and half of the sample frequency.
 *
 * Initialization is a 2-step process.  This makes it practical to change equalizer
 * levels on-the-fly.  The constructor starts up with a 4-tap FIR setup for direct
 * pass through.  Then the setup() in the .INO can specify the equalizer.
 * The newEqualizer() function has several parameters, the number of equalizer bands,
 * the frequencies of the bands, and the sidelobe level. All of these can be changed
 * dynamically.  This function can be changed dynamically, but it may be desireable to
 * mute the audio during the change to prevent clicks.
 *
 * Measured timing of update() for L or R 128 sample block, Teensy 3.6:
 *     Fixed time 13 microseconds
 *     Per FIR Coefficient time 2.5 microseconds (about 51E6 multiply and accumulates/sec)
 *     Total for 199 FIR Coefficients = 505 microseconds (17.4% of 44117 Hz available time)
 *     Total for stereo is twice those numbers.
 * Measured timing of update() for L or R 128 sample block, Teensy 4.0:
 *     Fixed time 1.4 microseconds
 *     Per FIR Coefficient time 0.44 microseconds (about 290E6 multiply and accumulate/sec)
 *     Total for 199 FIR Coefficients = 90 microseconds (3.1% of 44117 Hz available time)
 *     Total for stereo is twice those numbers
 * Measured for FIRGeneralNew(), T4.0, to design a 4000 tap FIR is 14 sec. This goes
 *     with the square of the number of taps.
 * Measured for getResponse() for nFIR=4000 and nFreq=5000, T4.0, is about a minute.
 *
 * Functions for the AudioFilterFIRGeneral_F32 object are
 *   FIRGeneralNew(*adb, nFIR, cf32f, kdb, *pStateArray); // to design and use an adb[]
 *      frequency response.
 *   LoadCoeffs(nFIR, cf32f, *pStateArray);    // To directly load FIR coefficients cf32f[]
 *   getResponse(nFreq, *rdb);   // To obtain the amplitude response in dB, rdb[]
 * 
 * Status: Tested T3.6, T4.0  No known bugs
 * 
 * Examples: TestFIRGeneralLarge4.ino   TestFIRGeneralLarge5.ino
 * 
 * Copyright (c) 2020 Bob Larkin
 * Any snippets of code from PJRC or Chip Audette used here brings with it
 * the associated license.
 *
 * In addition, work here is covered by MIT LIcense:
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

#ifndef _filter_FIR_general_f32_h
#define _filter_FIR_general_f32_h

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"
#include "mathDSP_F32.h"

#ifndef MF_PI
#define MF_PI 3.1415926f
#endif

// Temporary timing test
#define TEST_TIME_FIRG 0

#define ERR_FIRGEN_BANDS 1
#define ERR_FIRGEN_SIDELOBES 2
#define ERR_FIRGEN_NFIR 3

class AudioFilterFIRGeneral_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:filter_Equalizer
public:
    AudioFilterFIRGeneral_F32(void): AudioStream_F32(1,inputQueueArray) {
        // Initialize FIR instance (ARM DSP Math Library) with default simple passthrough FIR
        arm_fir_init_f32(&fir_inst, nFIR, (float32_t *)cf32f, &StateF32[0], (uint32_t)block_size);
    }
    AudioFilterFIRGeneral_F32(const AudioSettings_F32 &settings): AudioStream_F32(1,inputQueueArray) {
        block_size = settings.audio_block_samples;
        sample_rate_Hz = settings.sample_rate_Hz;
        arm_fir_init_f32(&fir_inst, nFIR, (float32_t *)cf32f, &StateF32[0], (uint32_t)block_size);
    }

    uint16_t FIRGeneralNew(float32_t *adb, uint16_t _nFIR, float32_t *_cf32f, float32_t kdb, float32_t *pStateArray);
    uint16_t LoadCoeffs(uint16_t _nFIR, float32_t *_cf32f, float32_t *pStateArray);
    void getResponse(uint16_t nFreq, float32_t *rdb);
    void update(void);

private:
    audio_block_f32_t *inputQueueArray[1];
    uint16_t block_size = AUDIO_BLOCK_SAMPLES;
    float32_t firStart[4] = {0.0, 1.0, 0.0, 0.0};  // Initialize to passthrough
    float32_t* cf32f = firStart;    // pointer to current coefficients
    uint16_t  nFIR  = 4;              // Number of coefficients
    float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;

    //  *Temporary* - TEST_TIME allows measuring time in microseconds for each part of the update()
#if TEST_TIME_FIRG
    elapsedMicros tElapse;
    int32_t iitt = 999000;     // count up to a million during startup
#endif
    // ARM DSP Math library filter instance
    arm_fir_instance_f32 fir_inst;
    float32_t StateF32[AUDIO_BLOCK_SAMPLES + 4];     // FIR_GENERAL_MAX_COEFFS];  // max, max
};
#endif
