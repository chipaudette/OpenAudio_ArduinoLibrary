/*     synth_sin_cos_f32.h
 *   AudioSynthSineCosine_F32
 *
 * Status: Checked for function and accuracy.  19 April 2020
 * 10 March 2021  Corrected Interpolation equations.  Bob L
 * 23 Feb 2022 Added "pure spectrum" filtering. RSL
 * This adds the function pureSpectrum(bool _setPure);   // Default: false
 *
 * Created: Bob Larkin 15 April 2020
 *
 * Based on Chip Audette's OpenAudio sine(), that was
 * Modeled on: AudioSynthWaveformSine from Teensy Audio Library
 *
 * Purpose: Create sine and cosine wave of given amplitude, frequency
 * and phase.  Outputs are audio_block_f32_t blocks of float32_t.
 * Routines are from the arm CMSIS library and use a 512 point lookup
 * table with linear interpolation to achieve float accuracy limits.
 *
 * This provides for setting the phase of the sine, setting the difference
 * in phase between the sine and cosine and setting the
 * (-amplitude, amplitude) range.  If these are at the default values,
 * called doSimple, the caluclation is faster.
 * For doSimple either true or false, the frequency can be changed
 * at will using the frequency() method.
 *
 * Defaults:
 * Frequency:  1000.0 Hz
 * Phase of Sine:  0.0 radians (0.0 deg)
 * Phase of Cosine:  pi/2 radians (90.0 deg) ahead of Sine
 * Amplitude:  -1.0 to 1.0
 *
 * Time: T3.6 update() block of 128 with doSimple is 36 microseconds
 *       Same using flexible doSimple=false is       49 microseconds
 *       T4.0 update() block of 128 with doSimple is 16 microseconds
 *       Same using flexible doSimple=false is       24 microseconds
 *
 * Copyright (c) 2020 Bob Larkin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef synth_sin_cos_f32_h_
#define synth_sin_cos_f32_h_

#include "AudioStream_F32.h"
#include "arm_math.h"

#ifndef M_PI
#define M_PI   3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2  1.57079632679489661923
#endif

#ifndef M_TWOPI
#define M_TWOPI  (M_PI * 2.0)
#endif

#define MF2_PI 6.2831853f

class AudioSynthSineCosine_F32 : public AudioStream_F32  {
//GUI: inputs:0, outputs:2 //this line used for automatic generation of GUI node
//GUI: shortName:SineCosine  //this line used for automatic generation of GUI node
public:
    AudioSynthSineCosine_F32(void) : AudioStream_F32(0, NULL) { } //uses default AUDIO_SAMPLE_RATE from AudioStream.h

    AudioSynthSineCosine_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL) {
	    setSampleRate_Hz(settings.sample_rate_Hz);
	    setBlockLength(settings.audio_block_samples);
    }

    void frequency(float32_t fr) {    // Frequency in Hz
		freq = fr;
        if (freq < 0.0f) freq = 0.0f;
        else if (freq > sample_rate_Hz/2.0f) freq = sample_rate_Hz/2.0f;
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
           arm_biquad_cascade_df1_init_f32( &bq_instS, 2, coeff32, state32S );
           arm_biquad_cascade_df1_init_f32( &bq_instC, 2, coeff32, state32C );
           }
        else
           {
           for(int ii=0; ii<10; ii++)  // Coeff for BiQuad BPF
              coeff32[ii] = 0.0;
           coeff32[0] = 1.0;           // b0 = 1 for pass through
           coeff32[5] = 1.0;
           arm_biquad_cascade_df1_init_f32( &bq_instS, 2, coeff32, state32S );
           arm_biquad_cascade_df1_init_f32( &bq_instC, 2, coeff32, state32C );
        }
    }

    /* Externally, phase comes in the range (0,2*M_PI) keeping with C math functions
     * Internally,  the full circle is represented as (0.0, 512.0).  This is
     * convenient for finding the entry to the sine table.
     */
    void phase_r(float32_t a) {
        while (a < 0.0f) a += MF2_PI;
        while (a > MF2_PI) a -= MF2_PI;
        phaseS = 512.0f * a / MF2_PI;
        doSimple = false;
        return;
    }

    // phaseS_C_r  is the number of radians that the cosine output leads the
    // sine output.  The default is M_PI_2 = pi/2 = 1.57079633 radians,
    // corresponding to 90.00 degrees cosine leading sine.
    void phaseS_C_r(float32_t a) {
        while (a < 0.0f) a += MF2_PI;
        while (a > MF2_PI) a -= MF2_PI;
        // Internally a full circle is 512.00 of phase
        phaseS_C = 512.0f * a / MF2_PI;
        doSimple = false;
        return;
    }

    // The amplitude, a, is the peak, as in zero-to-peak.  This produces outputs
    // ranging from -a to +a.  Both outputs are the same amplitude.
    void amplitude(float32_t a) {
        amplitude_pk = a;
        doSimple = false;
        return;
    }

     // Speed up calculations by setting phaseS_C=90deg, amplitude=1
     // Note, s=true will override any setting of phaseS_C_r or amplitude.
     void simple(bool s) {
        doSimple = s;
        if(doSimple) {
			phaseS_C = 128.0f;
			amplitude_pk = 1.0f;
	    }
        return;
    }

    void setSampleRate_Hz(float32_t fs_Hz) {
        // Check freq range
        if (freq > sample_rate_Hz/2.0f) freq = sample_rate_Hz/2.f;
        // update phase increment for new frequency
        phaseIncrement = 512.0f * freq / fs_Hz;
    }

    void setBlockLength(uint16_t bl) {
      if(bl > 128)  bl = 128;
      block_length = bl;
      }

    void pureSpectrum(bool _setPure) { doPureSpectrum = _setPure; }

    virtual void update(void);

private:
    float32_t freq = 1000.0f;
    float32_t phaseS = 0.0f;
    float32_t phaseS_C = 128.00;
    float32_t amplitude_pk = 1.0f;
    float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;
    float32_t phaseIncrement = 512.0f * freq /sample_rate_Hz;
    uint16_t  block_length = 128;
    // if only freq() is used, the complexities of phase, phaseS_C,
    // and amplitude are not used, speeding up the sin and cos:
    bool      doSimple = true;
    bool doPureSpectrum = false;   // Adds bandpass filter (not normally needed)
    float32_t coeff32[10];       // 2 biquad stages for filtering output shared S&C
    float32_t state32S[8];
    arm_biquad_casd_df1_inst_f32 bq_instS; // ARM DSP Math library filter instance.
    float32_t state32C[8];
    arm_biquad_casd_df1_inst_f32 bq_instC; // ARM DSP Math library filter instance.
};
#endif
