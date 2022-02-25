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
/* Revised 7 Feb 2022 to use a larger 512 point table and direct floating
 * point. The level of harmonics depends on the exact frequency, but seems
 * to be around -110 dB below the sine wave output.  This is more than
 * adequate for most applications.  For some testing, a pure sine wave,
 * limited only by the 24 bit mantissa, is useful.  For this, the function
 * pureSpectrum(true) will run two stages of biquad filtering putting the
 * harmonics below -135 dBc.  This filter tracks the frequency() entry, and
 * is available above a few hundred Hz, depending on the sample rate.  --Bob
 *
 * Update time is about 9 microsends for 128 update() with T4.x. This goes
 * up to 16 microseconds if "pureSpectrum" is used.
 */

#ifndef synth_sine2_f32_h_
#define synth_sine2_f32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"

class AudioSynthWaveformSine_F32 : public AudioStream_F32
{
//GUI: inputs:0, outputs:1 //this line used for automatic generation of GUI node
//GUI: shortName:sine  //this line used for automatic generation of GUI node
public:

    AudioSynthWaveformSine_F32() : AudioStream_F32(0, NULL), magnitude(0.5f) {
        initSine();
        } //uses default AUDIO_SAMPLE_RATE from AudioStream.h

    AudioSynthWaveformSine_F32(const AudioSettings_F32 &settings) :
                           AudioStream_F32(0, NULL), magnitude(0.5f) {
        setSampleRate_Hz(settings.sample_rate_Hz);
        initSine();
        }

    void initSine(void) {
        for(int ii=0; ii<10; ii++)  // Coeff for BiQuad BPF
           coeff32[ii] = 0.0;
        coeff32[0] = 1.0;           // b0 = 1 for pass through
        coeff32[5] = 1.0;
                                    // {numStages, pState, pCoeffs};
        arm_biquad_cascade_df1_init_f32( &bq_inst, 2, state32, coeff32 );
        }

   void frequency(float32_t _freq) {    // Frequency in Hz
        freq = _freq;
        if (freq < 0.0f)
           freq = 0.0f;
        if (freq > sample_rate_Hz/2.0f)
           freq = sample_rate_Hz/2.0f;
        phaseIncrement = 512.0f * freq / sample_rate_Hz;

        // Find coeff for 2 stages of BPF to remove harmoncs
        // Always compute these in case pureSpectrum is enabled later.
        if(freq > 0.003f*sample_rate_Hz)
           {
           float32_t q = 20.0f;
           float32_t w0 = freq * (2.0f * 3.141592654f / sample_rate_Hz);
           float32_t alpha = sin(w0) / (q * 2.0);
           float32_t scale = 1.0f / (1.0f + alpha);
           /* b0 */ coeff32[0] = alpha * scale;
           /* b1 */ coeff32[1] = 0;
           /* b2 */ coeff32[2] = (-alpha) * scale;
           /* a1 */ coeff32[3] = -(-2.0 * cos(w0)) * scale;
           /* a2 */ coeff32[4] = -(1.0 - alpha) * scale;
           /* b0 */ coeff32[5] = coeff32[0];
           /* b1 */ coeff32[6] = coeff32[1];
           /* b2 */ coeff32[7] = coeff32[2];
           /* a1 */ coeff32[8] = coeff32[3];
           /* a2 */ coeff32[9] = coeff32[4];
           arm_biquad_cascade_df1_init_f32( &bq_inst, 2, coeff32, state32 );
           }
        else
           {
           for(int ii=0; ii<10; ii++)  // Coeff for BiQuad BPF
              coeff32[ii] = 0.0;
           coeff32[0] = 1.0;           // b0 = 1 for pass through
           coeff32[5] = 1.0;
           arm_biquad_cascade_df1_init_f32( &bq_inst, 2, coeff32, state32 );
           enabled = false;
           }
    }

    /* Externally, phase comes in the range (.0, 360.0).
     * Internally,  the full circle is represented as (0.0, 512.0).  This is
     * convenient for finding the entry to the sine table.
     */
    void phase(float32_t _angle) {
        angle = 1.42222222f*_angle;  // Change (0,360) to (0, 512)
        while (angle < 0.0f) angle += 512.0f;
        while (angle > 512.0f) angle -= 512.0;
    }

    // The amplitude, a, is the peak, as in zero-to-peak.  This produces outputs
    // ranging from -a to +a.
    void amplitude(float32_t a) {
        if (a < 0.0f)  a = 0.0f;
        magnitude = a;
    }

    void setSampleRate_Hz(const float &fs_Hz) {
        phaseIncrement *= sample_rate_Hz / fs_Hz; //change the phase increment for the new frequency
        sample_rate_Hz = fs_Hz;
    }
    void begin(void) { enabled = true; }
    void end(void) { enabled = false; }
    void pureSpectrum(bool _setPure) { doPureSpectrum = _setPure; }
    virtual void update(void);

private:
    float32_t freq = 1000.0f;
    float32_t angle = 0.0f;        // Phase angle
    float32_t phaseS = 0.0f;
    float32_t phaseIncrement = 0.0f;
    float32_t magnitude = 0.0f;
    float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;
    bool doPureSpectrum = false;   // Adds bandpass filter (not normally needed)
    bool enabled = true;
    float32_t coeff32[10];       // 2 biquad stages for filtering output
    float32_t state32[8];
    arm_biquad_casd_df1_inst_f32 bq_inst; // ARM DSP Math library filter instance.
};
#endif
