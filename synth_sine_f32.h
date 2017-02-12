/* 
 * AdioSynthWaveformSine_F32
 * 
 * Created: Chip Audette (OpenAudio) Feb 2017
 * Modeled on: AudioSynthWaveformSine from Teensy Audio Library
 * 
 * Purpose: Create sine wave of given amplitude and frequency
 *
 * License: MIT License. Use at your own risk.        
 *
 */

#ifndef synth_sine_f32_h_
#define synth_sine_f32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"


class AudioSynthWaveformSine_F32 : public AudioStream_F32
{
public:
	AudioSynthWaveformSine_F32() : AudioStream_F32(0, NULL), magnitude(16384) {}
	void frequency(float freq) {
		if (freq < 0.0) freq = 0.0;
		else if (freq > AUDIO_SAMPLE_RATE_EXACT/2) freq = AUDIO_SAMPLE_RATE_EXACT/2;
		phase_increment = freq * (4294967296.0 / AUDIO_SAMPLE_RATE_EXACT);
	}
	void phase(float angle) {
		if (angle < 0.0) angle = 0.0;
		else if (angle > 360.0) {
			angle = angle - 360.0;
			if (angle >= 360.0) return;
		}
		phase_accumulator = angle * (4294967296.0 / 360.0);
	}
	void amplitude(float n) {
		if (n < 0) n = 0;
		else if (n > 1.0) n = 1.0;
		magnitude = n * 65536.0;
	}
	virtual void update(void);
private:
	uint32_t phase_accumulator;
	uint32_t phase_increment;
	int32_t magnitude;
};



#endif
