/*
 * AudioFilterEqualizer_F32
 *
 * Created: Bob Larkin   W7PUA   8 May 2020
 * 
 * This is a direct translation of the receiver audio equalizer written
 * by this author for the open-source DSP-10 radio in 1999.  See
 * http://www.janbob.com/electron/dsp10/dsp10.htm and
 * http://www.janbob.com/electron/dsp10/uhf3_35a.zip
 * 
 * Credit and thanks to PJRC, Paul Stoffregen and Teensy products for the audio
 * system and library that this is built upon as well as the float32
 * work of Chip Audette embodied in the OpenAudio_ArduinoLibrary. Many thanks
 * for the library structures and wonderful Teensy products.
 *
 * This equalizer is specified by an array of 'nBands' frequency bands
 * each of of arbitrary frequency span.  The first band always starts at
 * 0.0 Hz, and that value is not entered.  Each band is specified by the upper
 * frequency limit to the band.
 * The last band always ends at half of the sample frequency, which for 44117 Hz
 * sample frequency would be 22058.5.  Each band is specified by its upper
 * frequency in an .INO supplied array feq[].  The dB level of that band is
 * specified by a value, in dB, arranged in an .INO supplied array
 * aeq[].  Thus a trivial bass/treble control might look like:
 *   nBands = 3;
 *   feq[] = {300.0, 1500.0, 22058.5};
 *   float32_t bass = -2.5;  // in dB, relative to anything
 *   float32_t treble = 6.0;
 *   aeq[] = {bass, 0.0, treble};
 *
 * It may be obvious that this equalizer is a more general case of the common
 * functions such as low-pass, band-pass, notch, etc.   For instance, a pair
 * of band pass filters would look like:
 *   nBands = 5;
 *   feq[] = {500.0, 700.0, 2000.0, 2200.0, 22058.5};
 *   aeq[] = {-100.0, 0.0, -100.0, 2.0, -100.0};
 * where we added 2 dB of gain to the 2200 to 2400 Hz filter, relative to the 500
 * to 700 Hz band.
 *
 * An octave band equalizer is made by starting at some low frequency, say 40 Hz for the
 * first band.  The lowest frequency band will be from 0.0 Hz up to that first frequency.
 * Next multiply the first frequency by 2, creating in our example, a band from 40.0
 * to 80 Hz.  This is continued until the last frequency is about 22058 Hz.
 * This works out to require 10 bands, as follows:
 *   nBands = 10;
 *   feq[] = {    40.0, 80.0, 160.0, 320.0, 640.0, 1280.0, 2560.0, 5120.0, 10240.0, 22058.5};
 *   aeq[] = {  5.0,  4.0,  2.0,   -3.0, -4.0,  -1.0,    3.0, 6.0,    3.0,     0.5      };
 *
 * For a "half octave" equalizer, multiply each upper band limit by the square root of 2 = 1.414
 * to get the next band limit.  For that case, feq[] would start with a sequence
 * like 40, 56.56, 80.00, 113.1, 160.0, ... for a total of about 20 bands.
 *
 * How well all of this is achieved depends on the number of FIR coefficients
 * being used.  In the Teensy 3.6 / 4.0 the resourses allow a hefty number,
 * say 201, of coefficients to be used without stealing all the processor time
 * (see Timing, below).  The coefficient and FIR memory is sized for a maximum of
 * 250 coefficients, but can be recompiled for bigger with the define FIR_MAX_COEFFS.
 * To simplify calculations, the number of FIR coefficients should be odd.  If not
 * odd, the number will be reduced by one, quietly.
 *
 * If you try to make the bands too narrow for the number of FIR coeffficients,
 * the approximation to the desired curve becomes poor.  This can all be evaluated
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
 * This 16-bit integer version adjusts the maximum coefficient size to scale16 in the calls
 * to both equalizerNew() and getResponse().  Broadband equalizers can work with full-scale
 * 32767.0f sorts of levels, where narrow band filtering may need smaller values to
 * prevent overload. Experiment and check carefully.  Use lower values if there are doubts.
 * 
 * For a pass-through function, something like this (which can be intermixed with fancy equalizers):
 * float32_t fBand[] = {10000.0f, 22058.5f}; 
 * float32_t dbBand[] = {0.0f, 0.0f};
 * equalize1.equalizerNew(2, &fBand[0], &dbBand[0], 4, &equalizeCoeffs[0], 30.0f, 32767.0f);
 *  
 * Measured timing of update() for a 128 sample block, Teensy 3.6:
 *     Fixed time 13 microseconds
 *     Per FIR Coefficient time 2.5 microseconds
 *     Total for 199 FIR Coefficients = 505 microseconds (17.4% of 44117 Hz available time)
 * 
 *     Per FIR Coefficient, Teensy 4.0, 0.44 microseconds
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

#ifndef _filter_equalizer_f32_h
#define _filter_equalizer_f32_h

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"
#include "mathDSP_F32.h"

#ifndef MF_PI
#define MF_PI 3.1415926f
#endif

// Temporary timing test
#define TEST_TIME_EQ 0

#define EQUALIZER_MAX_COEFFS 251

#define ERR_EQ_BANDS 1
#define ERR_EQ_SIDELOBES 2
#define ERR_EQ_NFIR 3

class AudioFilterEqualizer_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:filter_Equalizer
    public:
        AudioFilterEqualizer_F32(void): AudioStream_F32(1,inputQueueArray) {
	        // Initialize FIR instance (ARM DSP Math Library) with default simple passthrough FIR
            arm_fir_init_f32(&fir_inst, nFIR, (float32_t *)cf32f,  &StateF32[0], (uint32_t)block_size);
		}
        AudioFilterEqualizer_F32(const AudioSettings_F32 &settings): AudioStream_F32(1,inputQueueArray) {
	        block_size = settings.audio_block_samples;
	        sample_rate_Hz = settings.sample_rate_Hz;
            arm_fir_init_f32(&fir_inst, nFIR, (float32_t *)cf32f,  &StateF32[0], (uint32_t)block_size);
		}

    uint16_t equalizerNew(uint16_t _nBands, float32_t *feq, float32_t *adb,
                      uint16_t _nFIR, float32_t *_cf32f, float32_t kdb);
    void getResponse(uint16_t nFreq, float32_t *rdb);
    void update(void);

private:
        audio_block_f32_t *inputQueueArray[1];
        uint16_t block_size = AUDIO_BLOCK_SAMPLES;
        float32_t firStart[4] = {0.0, 1.0, 0.0, 0.0};  // Initialize to passthrough
        float32_t* cf32f = firStart;    // pointer to current coefficients
        uint16_t  nFIR  = 4;              // Number of coefficients
        uint16_t nBands = 2;
        float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;
    
    //  *Temporary* - TEST_TIME allows measuring time in microseconds for each part of the update()
#if TEST_TIME_EQ
    elapsedMicros tElapse;
    int32_t iitt = 999000;     // count up to a million during startup
#endif
        // ARM DSP Math library filter instance
        arm_fir_instance_f32 fir_inst;
        float32_t StateF32[AUDIO_BLOCK_SAMPLES + EQUALIZER_MAX_COEFFS];  // max, max
};
#endif


