/*
 * AudioSynthWaveform_F32
 * 
 * Created: Patrick Radius, January 2017
 * Purpose: Generate waveforms at a given frequency and amplitude. Allows for pitch-modulation and portamento.
 *          
 * This processes a single stream fo audio data (ie, it is mono)       
 *          
 * MIT License.  use at your own risk.
*/
#ifndef SYNTHWAVEFORMF32_H
#define SYNTHWAVEFORMF32_H

#include <arm_math.h>
#include <AudioStream_F32.h>

class AudioSynthWaveform_F32 : public AudioStream_F32
{
  //GUI: inputs:0, outputs:1  //this line used for automatic generation of GUI node
  public:
    enum OscillatorMode {
        OSCILLATOR_MODE_SINE = 0,
        OSCILLATOR_MODE_SAW,
        OSCILLATOR_MODE_SQUARE,
        OSCILLATOR_MODE_TRIANGLE
    };

	    AudioSynthWaveform_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32), 
				_PI(2*acos(0.0f)),
                twoPI(2 * _PI),
				sample_rate_Hz(AUDIO_SAMPLE_RATE_EXACT),
                _OscillatorMode(OSCILLATOR_MODE_SINE),
                _Frequency(440.0f),
                _Phase(0.0f),
                _PhaseIncrement(0.0f),
                _PitchModAmt(0.0f),
                _PortamentoIncrement(0.0f),
                _PortamentoSamples(0),
                _CurrentPortamentoSample(0),
                _NotesPlaying(0)
		{		
			setSampleRate(settings.sample_rate_Hz);
		}
		
                
	
    AudioSynthWaveform_F32(void) : AudioStream_F32(1, inputQueueArray_f32),  //uses default AUDIO_SAMPLE_RATE from AudioStream.h
                _PI(2*acos(0.0f)),
                twoPI(2 * _PI),
				sample_rate_Hz(AUDIO_SAMPLE_RATE_EXACT),
                _OscillatorMode(OSCILLATOR_MODE_SINE),
                _Frequency(440.0f),
                _Phase(0.0f),
                _PhaseIncrement(0.0f),
                _PitchModAmt(0.0f),
                _PortamentoIncrement(0.0f),
                _PortamentoSamples(0),
                _CurrentPortamentoSample(0),
                _NotesPlaying(0) {};

    void frequency(float32_t freq) {
        float32_t nyquist = sample_rate_Hz/2.f;

        if (freq < 0.0) freq = 0.0;
        else if (freq > nyquist) freq = nyquist;

        if (_PortamentoSamples > 0 && _NotesPlaying > 0) {
          _PortamentoIncrement = (freq - _Frequency) / (float32_t)_PortamentoSamples;
          _CurrentPortamentoSample = 0;
        } else {
          _Frequency = freq;
        }

        _PhaseIncrement = _Frequency * twoPI / sample_rate_Hz;
    }

    void amplitude(float32_t n) {
        if (n < 0) n = 0;
        _magnitude = n;
    }

    void begin(short t_type) {
        _Phase = 0;
        oscillatorMode(t_type);
    }

    void begin(float32_t t_amp, float32_t t_freq, short t_type) {
        amplitude(t_amp);
        frequency(t_freq);
        begin(t_type);
    }

    void pitchModAmount(float32_t amount) {
      _PitchModAmt = amount;
    }

    void oscillatorMode(int mode) {
      _OscillatorMode = (OscillatorMode)mode;
    }

    void portamentoTime(float32_t slidetime) {
      _PortamentoTime = slidetime;
      _PortamentoSamples = floorf(slidetime * sample_rate_Hz);
    }

    
    void onNoteOn() {
      _NotesPlaying++;
    }

    void onNoteOff() {
      if (_NotesPlaying > 0) {
        _NotesPlaying--;
      }
    }

    void update(void);
	void setSampleRate(const float32_t fs_Hz)
	{
		_PhaseIncrement = _PhaseIncrement*sample_rate_Hz / fs_Hz;
		_PortamentoSamples = floorf( ((float)_PortamentoSamples) * fs_Hz / sample_rate_Hz );
		sample_rate_Hz = fs_Hz;
	}
  private:
    inline float32_t applyMod(uint32_t sample, audio_block_f32_t *lfo);
    const float32_t _PI;
    float32_t twoPI;
	float32_t sample_rate_Hz;
    
    OscillatorMode _OscillatorMode;
    float32_t _Frequency;
    float32_t _Phase;
    float32_t _PhaseIncrement;
    float32_t _magnitude;
    float32_t _PitchModAmt;
    float32_t _PortamentoTime;
    float32_t _PortamentoIncrement;

    uint64_t _PortamentoSamples;
    uint64_t _CurrentPortamentoSample;
    uint8_t _NotesPlaying;

    audio_block_f32_t *inputQueueArray_f32[1];
};

#endif