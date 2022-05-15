/* Hardware-SPDIF for Teensy 4
 * Copyright (c) 2019, Frank Bösing, f.boesing@gmx.de
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

#ifndef output_SPDIF3_f32_h_
#define output_SPDIF3_f32_h_

#include <Arduino.h>
#include "AudioStream_F32.h"
//include "AudioStream.h" 
#include <DMAChannel.h>

class AudioOutputSPDIF3_F32 : public AudioStream_F32
{
public:
	AudioOutputSPDIF3_F32(const AudioSettings_F32 &settings) : AudioStream_F32(2, inputQueueArray) {
		sample_rate_Hz = settings.sample_rate_Hz;
		begin();
	 }
	virtual void update(void);
	void begin(void);
	friend class AudioInputSPDIF3_F32;
	friend class AsyncAudioInputSPDIF3_F32;
	static void mute_PCM(const bool mute);
	static bool pll_locked(void);
protected:
	//AudioOutputSPDIF3_F32(int dummy): AudioStream(2, inputQueueArray) {}
	static void config_spdif3(float fs_Hz);
	static audio_block_f32_t *block_left_1st;
	static audio_block_f32_t *block_right_1st;
	static bool update_responsibility;
	static DMAChannel dma;
	static void isr(void);	
private:
	static uint32_t dpll_Gain() __attribute__ ((const));
	static audio_block_f32_t *block_left_2nd;
	static audio_block_f32_t *block_right_2nd;
	static audio_block_f32_t block_silent;
	audio_block_f32_t *inputQueueArray[2];
	static float sample_rate_Hz;
};


#endif
