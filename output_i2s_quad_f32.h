/*
 *  *****    output_i2s_quad_f32.h  *****
 *
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
/*
 *  Extended by Chip Audette, OpenAudio, May 2019
 *  Converted to F32 and to variable audio block length
 *	The F32 conversion is under the MIT License.  Use at your own risk.
 */
// Updated OpenAudio F32 with this version from Chip Audette's Tympan Library Jan 2021 RSL
// Removed old commented out code.  RSL 30 May 2022
// This version is 4 channel I2S output.  Greg Raven KF5N October, 2025

#ifndef output_i2s_quad_f32_h_
#define output_i2s_quad_f32_h_

#include <Arduino.h>
#include <arm_math.h>
#include "AudioStream_F32.h"
#include "DMAChannel.h"

class AudioOutputI2SQuad_F32 : public AudioStream_F32
{
	// GUI: inputs:4, outputs:0  //this line used for automatic generation of GUI node
public:
	// uses default AUDIO_SAMPLE_RATE and BLOCK_SIZE_SAMPLES from AudioStream_F32.h:
	AudioOutputI2SQuad_F32(void) : AudioStream_F32(4, inputQueueArray) { begin(); }
	// Allow variable sample rate and block size:
	AudioOutputI2SQuad_F32(const AudioSettings_F32 &settings) : AudioStream_F32(4, inputQueueArray)
	{
		sample_rate_Hz = settings.sample_rate_Hz;
		audio_block_samples = settings.audio_block_samples;
		half_block_length = audio_block_samples / 2;
		half_buffer_length = audio_block_samples * 2;
		begin();
	}

	// outputScale is a gain control for both left and right.  If set exactly
	// to 1.0f it is left as a pass-through.
	void setGain(float _oscale)
	{
		outputScale = _oscale;
	}
	virtual void update(void);
	void begin(void);
	void begin(bool);
	void scale_f32_to_i32(float32_t *p_f32, int len);

protected:
	AudioOutputI2SQuad_F32(int dummy) : AudioStream_F32(4, inputQueueArray) {} // to be used only inside AudioOutputI2Sslave !!

	inline static audio_block_f32_t *block_left_1st = nullptr;
	inline static audio_block_f32_t *block_right_1st = nullptr;
	inline static audio_block_f32_t *block_left_2nd = nullptr;
	inline static audio_block_f32_t *block_right_2nd = nullptr;
	inline static bool update_responsibility = false;
	static DMAChannel dma;
	static void isr(void);

private:
	static void config_i2s(void);
	static void config_i2s(bool);
	static void config_i2s(float);
	static void config_i2s(bool, float);
	audio_block_f32_t *inputQueueArray[4];
	inline static float sample_rate_Hz = AUDIO_SAMPLE_RATE;
	inline static int audio_block_samples = AUDIO_BLOCK_SAMPLES;
	inline static int half_block_length = AUDIO_BLOCK_SAMPLES / 2;
	inline static int half_buffer_length = AUDIO_BLOCK_SAMPLES * 2;
	const float32_t F32_TO_I32_NORM_FACTOR = 2147483647.0; // Which is 2^31-1.
	volatile uint8_t enabled = 1;
	float outputScale = 1.0f; // Quick volume control
};

class AudioOutputI2SQuadslave_F32 : public AudioOutputI2SQuad_F32
{
public:
	AudioOutputI2SQuadslave_F32(void) : AudioOutputI2SQuad_F32(0) { begin(); };
	void begin(void);
	friend class AudioInputI2Sslave_F32;
	friend void dma_ch0_isr(void);

protected:
	static void config_i2s(void);
};
#endif
