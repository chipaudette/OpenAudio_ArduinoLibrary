/*
	synth_whitenoise_F32
	
	Extended by: Chip Audette, OpenAudio, Feb 2017
	
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

#ifndef synth_whitenoise_f32_h_
#define synth_whitenoise_f32_h_
#include "Arduino.h"
#include "AudioStream.h"
#include "AudioStream_F32.h"
#include "utility/dspinst.h"

class AudioSynthNoiseWhite_F32 : public AudioStream_F32
{
//GUI: inputs:0, outputs:1 //this line used for automatic generation of GUI node
//GUI: shortName:whitenoise  //this line used for automatic generation of GUI node
public:
	AudioSynthNoiseWhite_F32() : AudioStream_F32(0, NULL) { setDefaultValues(); }
	AudioSynthNoiseWhite_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL) { setDefaultValues(); }
	
	void setDefaultValues(void) {
		level = 0;
		seed = 1 + instance_count++;
	}
	void amplitude(float n) {
		if (n < 0.0) n = 0.0;
		else if (n > 1.0) n = 1.0;
		level = (int32_t)(n * 65536.0);
	}
	virtual void update(void);
private:
	int32_t  level; // 0=off, 65536=max
	uint32_t seed;  // must start at 1
	static uint16_t instance_count;
};

#endif
