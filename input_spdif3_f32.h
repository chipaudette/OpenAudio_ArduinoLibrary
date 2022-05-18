/* Audio Library for Teensy 3.X
 * Copyright (c) 2019, Paul Stoffregen, paul@pjrc.com
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
// Frank Bösing

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)
#ifndef _input_spdif3_f32_h_
#define _input_spdif3_f32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "DMAChannel.h"

class AudioInputSPDIF3_F32 : public AudioStream_F32
{
public:
	AudioInputSPDIF3_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL) { 
		sample_rate_Hz = settings.sample_rate_Hz;
		begin(); 
	}
	virtual void update(void);
	void begin(void);
	static bool pllLocked(void);
	static unsigned int sampleRate(void);
protected:
	//AudioInputSPDIF3_F32(int dummy): AudioStream_F32(0, NULL) {} // to be used only inside AudioInputSPDIF3slave !!
	static bool update_responsibility;
	static DMAChannel dma;
	static void isr(void);
private:
	static audio_block_f32_t *block_left;
	static audio_block_f32_t *block_right;
	static uint16_t block_offset;
	static float sample_rate_Hz;
};

#endif
#endif
