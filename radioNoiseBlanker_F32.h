/* radioNoiseBlanker_F32.h
 * 15 May 2020     Bob Larkin
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

/*
 * An I-F signal comes in with occassional noise pulses.  This block watches for
 * the pulses and turns off the I-F while the pulse exists.  In order to
 * be as smart as possible this looks ahead by a number of samples, nAnticipation.
 * Likewise, the I-F is left off for nDecay samples after the pulse ends.
 * Various methods could be to be used to "turn off" the i-f,
 * including replacement with waveforms.
 * As of this initial write, zeros are used in the waveform.
 * 
 * A threshold can be adjusted via setNB().  This is compared with the
 * average rectified voltage being received.  If this is too small, like 1.5
 * or 2.0, we will be loosing data by blanking.  If we set it too high, like 20.0,
 * we will not blank noise pulses. Experiments will find a god setting.
 * With a sine wave input and no impulse noise, the average rectified signal
 * should be about 0.637.  To catch the top of that requires a threshold of
 * 1/0.637 = 1.57.  That would seem to be a very minimal threshold setting.
 *
 * Status: Tested on computer data
 * Functions:
 *   setNB( ... );  // See below
 *   showError(e);  // e=1 means to print update() error messages
 *   enable(runNB); // When runNB=false the signals bypasses the update() processing
 * Examples:
 *   TestNoiseBlanker1.ino  128 data for plotting and examination
 * Time: Update() of 128 samples 32 microseconds
* 
* Allow two channels, for I-Q receivers.  Input 0 operates gate.
* Paths 0 & 1 are gated.  31 March 2021.  Bob L.
 */

#ifndef _radio_noise_blanker_f32_h
#define _radio_noise_blanker_f32_h

#include "AudioStream_F32.h"
#include "arm_math.h"

#define RUNNING_SUM_SIZE 125

class radioNoiseBlanker_F32 : public AudioStream_F32 {
//GUI: inputs:2, outputs:2  //this line used for automatic generation of GUI node
//GUI: shortName: NoiseBlanker
public:
    // Option of AudioSettings_F32 change to block size (no sample rate dependent variables here):
    radioNoiseBlanker_F32(void) :  AudioStream_F32(2, inputQueueArray_f32) {
        block_size = AUDIO_BLOCK_SAMPLES;
    }
    radioNoiseBlanker_F32(const AudioSettings_F32 &settings) : AudioStream_F32(2, inputQueueArray_f32) {
        block_size = settings.audio_block_samples;
    }

    void enable(bool _runNB) {
        runNB = _runNB;
    } 

    // Channel 0 gates both Channel 0 & 1
    void useTwoChannel(bool _2Ch) {
        twoChannel = _2Ch;
    } 
    
    void setNoiseBlanker(float32_t _threshold, uint16_t _nAnticipation, uint16_t _nDecay) { 
	    if (_threshold < 0.0) threshold = 0.0;
	    else threshold = _threshold/(float32_t)RUNNING_SUM_SIZE;
	    
	    if(_nAnticipation < 1) nAnticipation = 1;
	    else if (_nAnticipation >125) nAnticipation = 125;
        else nAnticipation = _nAnticipation;
        
        if (_nDecay < 1)  nDecay = 1;
        else if (_nDecay > 10) nDecay = 10;  // Is this handled OK at end of block?
        else nDecay = _nDecay;
    }

    void showError(uint16_t e) {   // This is for debug
        errorPrint = e;
    }

    void update(void);

private:
    uint16_t block_size =  AUDIO_BLOCK_SAMPLES;
    // Input data pointers
    audio_block_f32_t *inputQueueArray_f32[2];

    // Control error printing in update() 0=No print
    uint16_t errorPrint = 0;

    // To look ahead we need a delay of up to 256 samples.  Too much removes good data.
    // Too little enters noise pulse data to the output. This can be a simple circular
    // buffer if we make the buffer a power of 2 in length and binary-truncate the index.
    // Choose 2^8 = 256.
    float32_t delayData0[256];   // The circular delay lines
    float32_t delayData1[256];
    uint16_t in_index = 0;      // Pointer to next block update entry
    // And a mask to make the circular buffer limit to a power of 2
    uint16_t delayBufferMask = 0X00FF;

    // Three variables to allow .INO control of Noise Blanker
    float32_t threshold = 1.0E3f;   // Start disabled
    uint16_t  nAnticipation = 5;
    uint16_t  nDecay = 8;
    
    int16_t   pulseTime = -9999; // Tracks nAnticipation and nDecay
    bool      gateOn = true;     // Signals going through NB
    float32_t runningSum = 0.0;
    bool      runNB = false;
    bool      twoChannel = false;  // Activates 2 channels for I-Q.
};
#endif
