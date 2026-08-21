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

#ifndef filter_variable_f32_h_
#define filter_variable_f32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"

class AudioFilterStateVariable_F32: public AudioStream_F32
{
public:
	AudioFilterStateVariable_F32() : AudioStream_F32(2, inputQueueArray) {
		frequency(1000);
		octaveControl(1.0); // default values
		resonance(0.707);
    limit(5.f);
		state_inputprev = 0;
		state_lowpass = 0;
		state_bandpass = 0;
	}
	void frequency(float freq) {
		if (freq < 20.0) freq = 20.0;
		else if (freq > AUDIO_SAMPLE_RATE_EXACT/2.5) freq = AUDIO_SAMPLE_RATE_EXACT/2.5;
		setting_fcenter = (freq * (3.141592654/(AUDIO_SAMPLE_RATE_EXACT*2.0)));
	}
	void resonance(float q) {
		if (q < 0.5) q = 0.5;
		else if (q > 5.0) q = 5.0;
		// TODO: allow lower Q when frequency is lower
		setting_damp = (1.0 / q);
	}
	void octaveControl(float n) {
		// filter's corner frequency is Fcenter * 2^(control * N)
		// where "control" ranges from -1.0 to +1.0
		// and "N" allows the frequency to change from 0 to 7 octaves
		if (n < 0.0) n = 0.0;
		else if (n > 6.9999) n = 6.9999;
		setting_octavemult = n;
	}

  void limit(float l)
  {
    if(l<0.f) l=0.f;
    setting_limit=l;
  }

	virtual void update(void);
private:
	void update_variable(const float *in, const float *ctl,
		float *lp, float *bp, float *hp);
	float setting_fcenter;
	float setting_octavemult;
	float setting_damp;
  float setting_limit;
	float state_inputprev;
	float state_lowpass;
	float state_bandpass;
	audio_block_f32_t *inputQueueArray[2];
};

#endif
