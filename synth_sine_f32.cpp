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
 * Revised per synth_sine_f32.h.  7 Feb 2022 Bob.
 */
#include "synth_sine_f32.h"
#include "utility/dspinst.h"
// 513 values of the sine wave in a float array:
#include "sinTable512_f32.h"

void AudioSynthWaveformSine_F32::update(void) {
    audio_block_f32_t *blockS;
    uint16_t index, i;
    float32_t a, b;

    blockS = AudioStream_F32::allocate_f32();   // Output blocks
    if (!blockS)  return;

    for (i=0; i < blockS->length; i++) {
       phaseS += phaseIncrement;
       if (phaseS > 512.0f)  phaseS -= 512.0f;
       index = (uint16_t) phaseS;
       float32_t deltaPhase = phaseS - (float32_t)index;
       /* Read two nearest values of input value from the sin table */
       a = sinTable512_f32[index];
       b = sinTable512_f32[index+1];
       blockS->data[i] = magnitude*(a+(b-a)*deltaPhase); // Linear interpolation
       }
    // For higher frequencies, an optional bandpass filter the output
    // This does a pass through for lower frequencies
    if(doPureSpectrum)
       arm_biquad_cascade_df1_f32(&bq_inst, blockS->data, blockS->data, 128);
    AudioStream_F32::transmit(blockS);
    AudioStream_F32::release (blockS);
}
