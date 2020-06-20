/* This is a direct translation of the Teensy Audio Library Peak Analyze
 * to use floating point f32.  This is intended to be compatible with
 * Chip Audette's floating point libraries.
 * Bob Larkin 24 April 2020  -  AudioAnalyze_Peak_F32
 *
 * Regard the copyright and the licensing:
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

 /* Input is a block of f32 numbers.  Output is the peak signal amplitude
  * calculated over multiple blocks.  Useful for audio level response projects,
  * and general troubleshooting.
  * 
  * Status:  Tested 3.6 and 4.0, no known bugs
  * Functions:
  *   available(); Returns true if new peak data is available.
  *   read();      Read the new peak value.
  *                Return is absolute + or - peak value in float.
  *   readPeakToPeak();  Same, but the differnce in + and - values.
  * Examples:
  *   TestPeakRMS.ino
  *   Similar: File > Examples > Audio > Analysis > PeakAndRMSMeterStereo
  * Time: Measured Teensy 3.6 time for the update() is 10 microseconds,
  *     for 128 block size. For Teensy 4.0 this is about 4 mcroseconds.
  *
  * The two variables, min_sample and max_sample, are checked until
  * an available() takes place. This is fine if the statistics of the input data are
  * stationary, i.e., not changing with time.  But, if the process is not
  * stationary, be aware and do available() & read() at appropriate times.
  */

#ifndef analyze_peak_f32_h_
#define analyze_peak_f32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"

class AudioAnalyzePeak_F32 : public AudioStream_F32 {
//GUI: inputs:1, outputs:0  //this line used for automatic generation of GUI node
//GUI: shortName: AnalyzePeak
public:
    AudioAnalyzePeak_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {
    }
    // Alternate specification of block size.  Sample rate does not apply for analyze_peak
    AudioAnalyzePeak_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) {
        block_size = settings.audio_block_samples;
    }

	bool available(void) {  // true means read() or readPeakToPeak() will return a new value
		__disable_irq();
		bool flag = new_output;
		if (flag) new_output = false;
		__enable_irq();
		return flag;
	}

    void showError(uint16_t e) {   // 0/1 Disables/Enables printing of update() errors
        errorPrint = e;
    }

    virtual void update(void);
    float32_t read(void);
    float32_t readPeakToPeak(void);

private:
    audio_block_f32_t *inputQueueArray_f32[1];
    uint16_t block_size = AUDIO_BLOCK_SAMPLES;
    volatile bool new_output = false;
    volatile bool just_read = true;
    float32_t min_sample;
    float32_t max_sample;

    // Control error printing in update().  Should never be enabled
    // until all audio objects have been initialized.
    // Only used as 0 or 1 now, but 16 bits are available.
    uint16_t errorPrint = 0;
};
#endif


