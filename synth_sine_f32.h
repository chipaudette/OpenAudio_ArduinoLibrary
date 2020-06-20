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
//GUI: inputs:0, outputs:1 //this line used for automatic generation of GUI node
//GUI: shortName:sine  //this line used for automatic generation of GUI node
public:
	AudioSynthWaveformSine_F32() : AudioStream_F32(0, NULL), magnitude(16384) { } //uses default AUDIO_SAMPLE_RATE from AudioStream.h
	AudioSynthWaveformSine_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL), magnitude(16384) {
		setSampleRate_Hz(settings.sample_rate_Hz);
	}
	void frequency(float freq) {
		if (freq < 0.0) freq = 0.0;
		else if (freq > sample_rate_Hz/2.f) freq = sample_rate_Hz/2.f;
		phase_increment = freq * (4294967296.0 / sample_rate_Hz);
	}
	void phase(float angle) {
		if (angle < 0.0f) angle = 0.0f;
		else if (angle > 360.0f) {
			angle = angle - 360.0f;
			if (angle >= 360.0f) return;
		}
		phase_accumulator = angle * (4294967296.0f / 360.0f);
	}
	void amplitude(float n) {
		if (n < 0) n = 0;
		else if (n > 1.0f) n = 1.0f;
		magnitude = n * 65536.0f;
	}
	void setSampleRate_Hz(const float &fs_Hz) {
		phase_increment *= sample_rate_Hz / fs_Hz; //change the phase increment for the new frequency
		sample_rate_Hz = fs_Hz;
	}
	void begin(void) { enabled = true; }
	void end(void) { enabled = false; }
	virtual void update(void);
	
private:
	uint32_t phase_accumulator = 0;
	uint32_t phase_increment = 0;
	int32_t magnitude = 0;
	float sample_rate_Hz = AUDIO_SAMPLE_RATE;
	volatile uint8_t enabled = 1;
};
#endif
