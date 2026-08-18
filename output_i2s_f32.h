/*
 *  *****    output_i2s_f32.h  *****
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

#ifndef output_i2s_f32_h_
#define output_i2s_f32_h_

#include <Arduino.h>
#include <arm_math.h>
#include "AudioStream_F32.h"
#include "DMAChannel.h"

class AudioOutputI2S_F32 : public AudioStream_F32
{
//GUI: inputs:2, outputs:0  //this line used for automatic generation of GUI node
public:
    //uses default AUDIO_SAMPLE_RATE and BLOCK_SIZE_SAMPLES from AudioStream.h:
	AudioOutputI2S_F32(void) : AudioStream_F32(2, inputQueueArray)	{ begin();}
	// Allow variable sample rate and block size:
	AudioOutputI2S_F32(const AudioSettings_F32 &settings) : AudioStream_F32(2, inputQueueArray)
	{
		sample_rate_Hz = settings.sample_rate_Hz;
		audio_block_samples = settings.audio_block_samples;
		begin();
	}
	// Force the I2S slot word width.  Valid values: 16, 24, 32.
	// 0 (the default) means 32-bit slots in master mode, or auto-detection
	// from the external clock's BCLK/FS ratio in sink mode.
	//   16-bit master: short frames, 32*fs bit clock
	//   24-bit master: short frames, 48*fs bit clock (192*fs MCLK)
	AudioOutputI2S_F32(int word_width) : AudioStream_F32(2, inputQueueArray)
	{
		expected_word_width = word_width;
		begin();
	}
	// Variable sample rate and block size, plus forced slot word width:
	AudioOutputI2S_F32(const AudioSettings_F32 &settings, int word_width) : AudioStream_F32(2, inputQueueArray)
	{
		sample_rate_Hz = settings.sample_rate_Hz;
		audio_block_samples = settings.audio_block_samples;
		expected_word_width = word_width;
		begin();
	}
	// Force a slot word width (16/24/32 bits) and skip auto-detection.
	// Pass 0 (the default) for 32-bit master slots or sink auto-detect.
	static void setExpectedWordWidth(int w) { expected_word_width = w; }

    // outputScale is a gain control for both left and right.  If set exactly
    // to 1.0f it is left as a pass-through.
    void setGain(float _oscale) {
       outputScale = _oscale;
       }
	virtual void update(void);
	void begin(void);
	void begin(bool);
	friend class AudioInputI2S_F32;
	friend class AudioInputI2Ssink_F32;
	#if defined(__IMXRT1062__)
	friend class AudioOutputI2SQuad_F32;
	friend class AudioInputI2SQuad_F32;
	//friend class AudioOutputI2SHex;
	//friend class AudioInputI2SHex;
	//friend class AudioOutputI2SOct;
	//friend class AudioInputI2SOct;
	#endif

	static void scale_f32_to_i16( float32_t *p_f32, float32_t *p_i16, int len) ;
	static void scale_f32_to_i24( float32_t *p_f32, float32_t *p_i16, int len) ;
	static void scale_f32_to_i32( float32_t *p_f32, float32_t *p_i32, int len) ;

	static float setI2SFreq_T3(const float);  // I2S clock for T3,x
	// In sink mode the slot word width is auto-detected from the external
	// clock's BCLK/FS ratio (16/24/32-bit).  Returns the width in bits.
	static int getDetectedWordWidth(void) { return word_width; }
	// Sink mode only: re-run the BCLK/FS ratio probe and re-apply the detected
	// slot word width to SAI1.  The constructor's probe runs during static
	// init (before the external clock source may have started) and can fall
	// back to 32-bit slots; call this from setup() once the clock source is
	// confirmed running to pick up short frames.  Overrides a forced width.
	// Returns the detected width in bits.
	static int detectWordWidth(void);
	// Diagnostics from the last probe: number of frame syncs seen (0 = no
	// external clock present) and words per frame (4/6/8 -> 16/24/32-bit).
	static int getProbeFrameCount(void);
	static int getProbeWordsPerFrame(void);
protected:
	AudioOutputI2S_F32(bool sinkMode, int word_width) : AudioStream_F32(2, inputQueueArray)
	{
		expected_word_width = word_width;
	} // to be used only inside AudioOutputI2Ssink !!
	static void config_i2s(void);
	static void config_i2s(bool);
	static void config_i2s(float);
	static void config_i2s(bool, float);
	static audio_block_f32_t *block_left_1st;
	static audio_block_f32_t *block_right_1st;
	static bool update_responsibility;
	static DMAChannel dma;
	static void isr_16(void);
	static void isr_32(void);
	static void isr(void);
protected:
	static float sample_rate_Hz;
	static int audio_block_samples;
	static int word_width;            // detected slot word width in bits (16/24/32)
	static int expected_word_width;   // forced width (0 = auto-detect)
private:
	static audio_block_f32_t *block_left_2nd;
	static audio_block_f32_t *block_right_2nd;
	static uint16_t block_left_offset;
	static uint16_t block_right_offset;
	audio_block_f32_t *inputQueueArray[2];
	volatile uint8_t enabled = 1;
    float outputScale = 1.0f;  // Quick volume control
};

// I2S "sink": outputs audio on SAI1 (data out = pin 7) but does NOT generate the
// bit clock or frame sync.  BCLK (pin 21) and FS (pin 20) must be driven by an
// external clock source (another Teensy, a codec in its clock-source role, etc.).
// The slot word width (16/24/32-bit) is auto-detected from the external clock's
// BCLK/FS ratio at startup, or forced with the constructor (or
// AudioOutputI2S_F32::setExpectedWordWidth).  On Teensy 4.x this uses full
// 32-bit data in each slot.  On Teensy 3.x it uses the original 16-bit-in-the-
// upper-half packing.  The DMA ISR drives update_all(), so the audio block
// scheduler is clocked by the external clock source.
class AudioOutputI2Ssink_F32 : public AudioOutputI2S_F32
{
public:
	AudioOutputI2Ssink_F32(void) : AudioOutputI2S_F32(true, 0) { begin(); } ;
	AudioOutputI2Ssink_F32(int word_width) : AudioOutputI2S_F32(true, word_width) { begin(); } ;
	void begin(void);
	friend class AudioInputI2Ssink_F32;
	friend void dma_ch0_isr(void);
protected:
	static void config_i2s(void);
};
#endif
