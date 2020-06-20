/* This is a direct translation of the Teensy Audio Library RMS Analyze
 * to use floating point f32.  This is intended to be compatible with
 * Chip Audette's floating point libraries.
 * Bob Larkin 23 April 2020  -  AudioAnalyze_RMS_F32
 *
 * Regard the copyright and licensing:
 * Audio Library for Teensy 3.X
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

 /* Input is a block of f32 numbers.  Output is the signal RMS amplitude
  * calculated over multiple blocks.  Useful for audio level response projects,
  * and general troubleshooting.
  * 
  * Status: Tested 3.6 and T4.0  No known bugs.
  * Functions:
  *   available();  Returns true if new RMS data is available.
  *   read();       Read the new RMS value. Return is rms value in float.
  * Examples:    
  *   TestPeakRMS.ino
  *   Similar: File > Examples > Audio > Analysis > PeakAndRMSMeterStereo
  * Time: Measured time for the update of F32 RMS is 7 microseconds
  *     for 128 block size.
  *
  * The average "power" is found by squaring the sample values, adding them
  * together and dividing by the number of samples.  The RMS value is a "voltage"
  * found by taking the square root of the power.  This is in DSP lingo, and
  * the units are vague. But, since usually, only relative values are important,
  * the units will cancel out!
  *
  * The two variables, accum and count, keep adding to the power sum until
  * a read() takes place. This is fine if the statistics of the input data are
  * stationary, i.e., not changing with time.  But, if the process is not
  * stationary, be aware and do read() at appropriate times.
  */

#ifndef analyze_rms_f32_h_
#define analyze_rms_f32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"

class AudioAnalyzeRMS_F32 : public AudioStream_F32 {
//GUI: inputs:1, outputs:0  //this line used for automatic generation of GUI node
//GUI: shortName: AnalyzeRMS
public:
    AudioAnalyzeRMS_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {
		// default values from initialization below
    }
    // Alternate specification of block size.  Sample rate does not apply for analyze_rms
    AudioAnalyzeRMS_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) {
        block_size = settings.audio_block_samples;
    }

    bool available(void) {
        return count > 0;
    }

    void showError(uint16_t e) {  // 0/1 Disables/Enables printing of update() errors
        errorPrint = e;
    }

    float read(void);

    virtual void update(void);

private:
    audio_block_f32_t *inputQueueArray_f32[1];
    uint16_t block_size = AUDIO_BLOCK_SAMPLES;
    // double for accum is very safe, but needed? Well, power dynamic Range x 1 sec of data =~ 1E7 x 4E4 = 4E11
    // whereas float range for positive numbers is 2^23 =~ 1E7 so double is easily justified for accuracy. 
    // The timing 7 microseconds per 128, includes using double for accum, so the price is reasonable.
    double accum = 0.0;
       uint32_t count = 0;

    // Control error printing in update().  Should never be enabled
    // until all audio objects have been initialized.
    // Only used as 0 or 1 now, but 16 bits are available.
    uint16_t errorPrint = 0;
};
#endif


