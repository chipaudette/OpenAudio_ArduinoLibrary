#include <arm_math.h>
#include "synth_waveform_F32.h"

void AudioSynthWaveform_F32::update(void) {
  audio_block_f32_t *block, *lfo;

  if (_magnitude == 0.0f) return;

  block = allocate_f32();
  if (!block) return;

  lfo = receiveReadOnly_f32(0);
  switch (_OscillatorMode) {
    case OSCILLATOR_MODE_SINE:
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          applyMod(i, lfo);

          block->data[i] = arm_sin_f32(_Phase);
          _Phase += _PhaseIncrement;
          while (_Phase >= twoPI) {
              _Phase -= twoPI;
          }
        }
        break;
    case OSCILLATOR_MODE_SAW:
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          applyMod(i, lfo);

          block->data[i] = 1.0f - (2.0f * _Phase / twoPI);
          _Phase += _PhaseIncrement;
          while (_Phase >= twoPI) {
            _Phase -= twoPI;
          }
        }
        break;
    case OSCILLATOR_MODE_SQUARE:
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        applyMod(i, lfo);

        if (_Phase <= _PI) {
          block->data[i] = 1.0f;
        } else {
          block->data[i] = -1.0f;
        }

        _Phase += _PhaseIncrement;
        while (_Phase >= twoPI) {
          _Phase -= twoPI;
        }
      }
      break;
    case OSCILLATOR_MODE_TRIANGLE:
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        applyMod(i, lfo);

        float32_t value = -1.0f + (2.0f * _Phase / twoPI);
        block->data[i] = 2.0f * (fabs(value) - 0.5f);
        _Phase += _PhaseIncrement;
        while (_Phase >= twoPI) {
          _Phase -= twoPI;
        }
      }
      break;
  }

  if (_magnitude != 1.0f) {
    arm_scale_f32(block->data, _magnitude, block->data, AUDIO_BLOCK_SAMPLES);
  }

  if (lfo) {
    release(lfo);
  }

  AudioStream_F32::transmit(block);
  AudioStream_F32::release(block);
}

inline float32_t AudioSynthWaveform_F32::applyMod(uint32_t sample, audio_block_f32_t *lfo) {
  if (_PortamentoSamples > 0 && _CurrentPortamentoSample++ < _PortamentoSamples) {
    _Frequency+=_PortamentoIncrement;
  }

  float32_t osc_frequency = _Frequency;

  if (lfo && _PitchModAmt > 0.0f) {
    osc_frequency = _Frequency * powf(2.0f, 0.0f / 1200.0f + lfo->data[sample] * _PitchModAmt);
  }

  _PhaseIncrement = osc_frequency * twoPI / AUDIO_SAMPLE_RATE_EXACT;

  return osc_frequency;
}