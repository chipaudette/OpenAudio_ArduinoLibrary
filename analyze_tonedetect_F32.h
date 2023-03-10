/*
 * analyze_tonedetect_F32.h  Converted to float from PJRC Teensy Audio Library
 * for the OpenAudio_TeensyArduino library (floating point audio).
 *   MIT License on changed portions
 *   Bob Larkin March 2021
 *
 * See also  analyze_CTCSS_F32  that is specific for the CTCSS tone system
 * with tones in the 67.0 to 250.3 Hz range.  (In this same library)
 *
 * Originally from:  Audio Library for Teensy 3.X
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
 // Added setGain(gain) March 2023  Bob L

#ifndef analyze_tonedetect_F32_h_
#define analyze_tonedetect_F32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"

class AudioAnalyzeToneDetect_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:0  //this line used for automatic generation of GUI node
//GUI: shortName:ToneDet
public:
    AudioAnalyzeToneDetect_F32(void)
      : AudioStream_F32(1, inputQueueArray_f32), thresh(0.1f), enabled(false) {
          sample_rate_Hz = AUDIO_SAMPLE_RATE;
          block_size = AUDIO_BLOCK_SAMPLES;
          }
    // Option of AudioSettings_F32 change to block size and/or sample rate:
    AudioAnalyzeToneDetect_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32),
        thresh(0.1f), enabled(false) {
        sample_rate_Hz = settings.sample_rate_Hz;
        block_size = settings.audio_block_samples;
        }

    void frequency(float freq, uint16_t cycles=10) {
        set_params( 2.0f*cosf(freq*6.28318530718f/sample_rate_Hz),
           cycles,
           (uint16_t)( 0.5f + sample_rate_Hz*(float)cycles/freq) );
           //(uint16_t)( ( (float)sample_rate_Hz/freq*(float)cycles) + 0.5f) );
        }

    void setGain(float _gain) {
        gain = _gain;
        }

    bool available(void) {
        __disable_irq();
        bool flag = new_output;
        if(flag)
            new_output = false;
        __enable_irq();
        return flag;
        }

    float read(void);

    void threshold(float level) {
        if (level < 0.01f) thresh = 0.01f;
        else if (level > 0.99f) thresh = 0.99f;
        else thresh = level;
        }

    operator bool();  // true if at or above threshold, false if below

    void set_params(float coef, uint16_t cycles, uint16_t len);

    virtual void update(void);

private:
    float coefficient;       // Goertzel algorithm coefficient
    float s1, s2;            // Goertzel algorithm state
    float out1, out2;        // Goertzel algorithm state output
    float power;
    float gain = 1.0f;       // Voltage gain, added Mar 2023
    uint16_t length;         // number of samples to analyze
    uint16_t count;          // how many left to analyze
    uint16_t ncycles;        // number of waveform cycles to seek
    uint16_t thresh = 0.1f;  // threshold, 0.01 to 0.99
    bool enabled = false;
    volatile bool new_output = false;
    audio_block_f32_t *inputQueueArray_f32[1];
    float sample_rate_Hz = AUDIO_SAMPLE_RATE;
    uint16_t block_size = 128;
};

#endif
