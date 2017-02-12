/* 
 * AudioSynthWaveformSine_F32
 * 
 * Created: Chip Audette (OpenAudio) Feb 2017
 * Modeled on: AudioSynthWaveformSine from Teensy Audio Library
 * 
 * Purpose: Create sine wave of given amplitude and frequency
 *
 * License: MIT License. Use at your own risk.
 *
 */

#include "synth_sine_f32.h"
#include "utility/dspinst.h"

// data_waveforms.c
extern "C" {
  extern const int16_t AudioWaveformSine[257];
}


void AudioSynthWaveformSine_F32::update(void)
{
	audio_block_f32_t *block;
	uint32_t i, ph, inc, index, scale;
	int32_t val1, val2;

	if (magnitude) {
		block = allocate_f32();
		if (block) {
			ph = phase_accumulator;
			inc = phase_increment;
			for (i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
				index = ph >> 24;
				val1 = AudioWaveformSine[index];
				val2 = AudioWaveformSine[index+1];
				scale = (ph >> 8) & 0xFFFF;
				val2 *= scale;
				val1 *= 0x10000 - scale;
#if defined(KINETISK)
				block->data[i] = (float) multiply_32x32_rshift32(val1 + val2, magnitude);
#elif defined(KINETISL)
				block->data[i] = (float) ((((val1 + val2) >> 16) * magnitude) >> 16);
#endif
				ph += inc;
				
				block->data[i] = block->data[i] / 32768.0f; // scale to float
			}
			phase_accumulator = ph;
			
			
			
			AudioStream_F32::transmit(block);
			AudioStream_F32::release(block);
			return;
		}
	}
	phase_accumulator += phase_increment * AUDIO_BLOCK_SAMPLES;
}






