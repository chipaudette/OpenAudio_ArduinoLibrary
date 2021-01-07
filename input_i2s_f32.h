/*
 *  *****  input_i2s_f32.h  ******
 * 
 *  Audio Library for Teensy 3.X
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

#ifndef _input_i2s_f32_h_
#define _input_i2s_f32_h_

#include <Arduino.h>
#include <arm_math.h> 
#include "AudioStream_F32.h"
#include "AudioStream.h"   //Do we really need this?? (Chip, 2020-10-31)
#include "DMAChannel.h"

class AudioInputI2S_F32 : public AudioStream_F32
{
//GUI: inputs:0, outputs:2  //this line used for automatic generation of GUI nodes
public:
	AudioInputI2S_F32(void) : AudioStream_F32(0, NULL) { begin(); } //uses default AUDIO_SAMPLE_RATE and BLOCK_SIZE_SAMPLES from AudioStream.h
	AudioInputI2S_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL) { 
		sample_rate_Hz = settings.sample_rate_Hz;
		audio_block_samples = settings.audio_block_samples;
		begin(); 
	}
	
	virtual void update(void);
	static void scale_i16_to_f32( float32_t *p_i16, float32_t *p_f32, int len) ;
	static void scale_i24_to_f32( float32_t *p_i24, float32_t *p_f32, int len) ;
	static void scale_i32_to_f32( float32_t *p_i32, float32_t *p_f32, int len);
	void begin(void);
	void begin(bool);
	void sub_begin_i32(void);
	//void sub_begin_i16(void);
	int get_isOutOfMemory(void) { return flag_out_of_memory; }
	void clear_isOutOfMemory(void) { flag_out_of_memory = 0; }
	//friend class AudioOutputI2S_F32;
protected:	
	AudioInputI2S_F32(int dummy): AudioStream_F32(0, NULL) {} // to be used only inside AudioInputI2Sslave !!
	static bool update_responsibility;
	static DMAChannel dma;
	static void isr_32(void);
	static void isr(void);
	virtual void update_1chan(int, audio_block_f32_t *&);
private:
	static audio_block_f32_t *block_left_f32;
	static audio_block_f32_t *block_right_f32;
	static float sample_rate_Hz;
	static int audio_block_samples;
	static uint16_t block_offset;
	static int flag_out_of_memory;
	static unsigned long update_counter;
};

class AudioInputI2Sslave_F32 : public AudioInputI2S_F32
{
public:
	AudioInputI2Sslave_F32(void) : AudioInputI2S_F32(0) { begin(); }
	void begin(void);
	friend void dma_ch1_isr(void);
};
#endif
