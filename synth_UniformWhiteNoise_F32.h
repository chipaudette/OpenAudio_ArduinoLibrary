/*       synth_UniformWhiteNoise_F32.h  ---  New July 2025
   Generates a block of uniformly distributed, uncorrelated, white
   noise of a range of (-level, +level) as set by amplitude(level)
   with a default value of level = 0.0.  This replaces the class
   synth_whitenoise_F32 and corrects the need for I16 memory as well
   providing output that uses the full F32 mantissa of 24-bits.

   Method is xor shift 64 credited to George Marsaglia.   See
   https://en.wikipedia.org/wiki/Xorshift   and
   https://forum.pjrc.com/index.php?threads/teensy-4-1-random-number-generator.61125
   post 13 by #Nominal Animal.
     Bob Larkin July 2025.  Thanks #grrrr

	Derived from synth_whitenoise_F32 that was
	extended by: Chip Audette, OpenAudio, Feb 2017
	
	License: MIT License.  Use at your own risk.
*/
/* Audio Library for Teensy 3.X
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

// ** This does not support Teensy LC (KINETISL).  Teensy 3.x and 4.x OK.  **
// Execution time 15 uSec for 128 size block on T4.x. 12 of these are
// the conversionfrom uint32_t to F32 with offset for DC=0.

#ifndef synth_uniform_white_noise_f32_h_
#define synth_uniform_white_noise_f32_h_
#include "Arduino.h"
#include "AudioStream_F32.h"
#include "utility/dspinst.h"

class AudioSynthUniformWhiteNoise_F32 : public AudioStream_F32
{
//GUI: inputs:0, outputs:1 //this line used for automatic generation of GUI node
//GUI: shortName:uniformwhitenoise  //this line used for automatic generation of GUI node
public:
	AudioSynthUniformWhiteNoise_F32() : AudioStream_F32(0, NULL) { setDefaultValues(); }
	AudioSynthUniformWhiteNoise_F32(const AudioSettings_F32 &settings) :
         AudioStream_F32(0, NULL) { setDefaultValues(); }
	void setDefaultValues(void) {
		level = 0.0f;
		rnState = 1ULL + instance_count++;
	}

	void amplitude(float nL) {
		if (nL < 0.0) nL = 0.0;
      level = nL;
	}

	virtual void update(void);

private:
   float32_t level = 0.0f;
	uint64_t rnState;
	uint64_t instance_count=0ULL;
};
#endif
