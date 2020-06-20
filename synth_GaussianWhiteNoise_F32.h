/*
 *  synth_GaussianWhiteNoise_F32.h
 *  W7PUA 15 June 2020
 *
 * Thanks to PJRC and Pau Stoffregen for the Teensy processor, Teensyduino
 * and Teensy Audio.  Thanks to Chip Audette for the F32 extension
 * work.  Alll of that makes this possible.  Bob
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
/* This noise source is very close to a Gaussian distribution with mean of
 * zero and a standard deviation specified by amplitude().  Default amlitude
 * is 0.0 (off). Time requirements:
 *   For generating a block of 128, Teensy 3.6, 121 microseconds
 *   For generating a block of 128, Teensy 4.0,  36 microseconds
 */

#ifndef synth_GaussianWhiteNoise_f32_h_
#define synth_GaussianWhiteNoise_f32_h_
#include "Arduino.h"
#include "AudioStream_F32.h"

// Temporary timing test
#define TEST_TIME_GWN 0

#define FL_ONE  0X3F800000
#define FL_MASK 0X007FFFFF

class AudioSynthGaussian_F32 : public AudioStream_F32
{
//GUI: inputs:0, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:Gaussianwhitenoise  //this line used for automatic generation of GUI node
public:
    AudioSynthGaussian_F32() : AudioStream_F32(0, NULL) { }
    // Allow for changing block size?
    AudioSynthGaussian_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL) { }

    // Gaussian amplitude is specified by the 1-sigma (standard deviation) value.
    // sd=0.0 is un-enabled.
    void amplitude(float _sd) {
        sd = _sd;  // Enduring copy
        if (sd<0.0)  sd=0.0;
    }
    // Add printError(uint16_t e)  - not currently used.
    virtual void update(void);

private:
    uint16_t blockSize = AUDIO_BLOCK_SAMPLES;
    uint32_t idum = 12345;
    float32_t sd = 0.0;  // Default to off
    uint16_t errorPrint = 0;
    //  *Temporary* - TEST_TIME allows measuring time in microseconds for each part of the update()
#if TEST_TIME_GWN
    elapsedMicros tElapse;
    int32_t iitt = 999000;     // count up to a million during startup
#endif
};
#endif
