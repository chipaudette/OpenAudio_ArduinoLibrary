/*     radioModulatedGenerator_F32.h
 *
 *   RadioModulatedGenerator_F32 class
 *
 * Created: Bob Larkin 15 April 2021
 *
 * For AM, the input is the 0 (left) channel. 100% AM modulation corresponds
 * to this input -1.0 to 1.0.  Overmodulation (more that 100%) results in peak
 * increases beyond twice amplitude, but full abrupt clipping at the
 * bottom zero point.  Clipping on the top would be in an external block,
 * if desired
 *
 * For PM or FM (only one at a time) the input goes to the 1 channel.  For PM,
 * the input level corresponds to radians of phase change, + or -.  For FM,
 * the input correspondss to Hz of deviation (or as modified by deviationFMMultiplier).
 *
 * For digital modulation, such as QAM, there can be both phase and amplitude
 * modulation.  This would be set by
 *      doModulation_AM_PM_FM(true, true, false, bool _bothIQ)
 *
 * If _bothIQ is false, the output is all at channel 0.  This is a standard
 * modulated waveform as would be transmitted by wires or radio.  If _bothIQ
 * is true, a pair of outputs on channels 0 and 1 correspond to I and Q
 * components, as would be used with "phasing mixers" to convert the transmit
 * frequency.
 *
 * Amplitude and phase corrections can be applied when there I-Q outputs.
 * This can compensate for errors in the external hardware.  See the functions:
 *      phaseQ_I(float32_t ph)
 *      amplitudeQ_I(float32_t _a)
 *
 * Time: T3.6 update() block of 128 is about 53 microseconds  AM Single output
 *       T4.x update() block of 128 is about 20 microseconds  AM Single output
 *       T4.x update() block of 128 is about 35 microseconds  AM I + Q outputs
 *       For T4.x, FM is 1 or 2 microseconds faster than AM.
 *
 * Copyright (c) 2021 Bob Larkin
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

#ifndef modulate_AM_PM_FM_f32_h_
#define modulate_AM_PM_FM_f32_h_

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
#define K512ON2PI 81.487331f

class radioModulatedGenerator_F32 : public AudioStream_F32  {
//GUI: inputs:2, outputs:2 //this line used for automatic generation of GUI node
//GUI: shortName:Modulator  //this line used for automatic generation of GUI node
public:
    radioModulatedGenerator_F32(void) : AudioStream_F32(2, inputQueueArray_f32) { } //uses default AUDIO_SAMPLE_RATE from AudioStream.h
    radioModulatedGenerator_F32(const AudioSettings_F32 &settings) : AudioStream_F32(2, inputQueueArray_f32) {
        setSampleRate_Hz(settings.sample_rate_Hz);
        setBlockLength(settings.audio_block_samples);
    }

    void frequency(float32_t fr) {    // Center Frequency in Hz
        freq = fr;
        if (freq < 0.0f) freq = 0.0f;
        else if (freq > sample_rate_Hz/2.0f) freq = sample_rate_Hz/2.0f;
        phaseIncrement0 = 512.0f * freq / sample_rate_Hz;
    }

    /* Externally, phase comes in the range (0,2*M_PI) keeping with C math functions
     * Internally,  the full circle is represented as (0.0, 512.0).  This is
     * convenient for finding the entry to the sine table.
     */
    void phase_r(float32_t ph) {
        while (ph < 0.0f) ph += MF2_PI;
        while (ph > MF2_PI) ph -= MF2_PI;
        phaseS = 512.0f * ph / MF2_PI;
        return;
    }

    // A deviationFMScale of 1000.0 causes the deviation in FM to
    // * be 1000 Hz/unit instead of 1 Hz/unit.
    // */
    void setFMScale(float mult) {
		deviationFMScale = mult;
	}

    // phaseQ_I  is the number of radians that the cosine output leads the
    // sine output.  The default is M_PI_2 = pi/2 = 1.57079633 radians,
    // corresponding to 90.00 degrees cosine leading sine.
    void phaseQ_I_r(float32_t ph) {
        while (ph < 0.0f) ph += MF2_PI;
        while (ph > MF2_PI) ph -= MF2_PI;
        // Internally a full circle is 512.00 of phase
        phaseQ_I = 512.0f * ph / MF2_PI;
        return;
    }

    // amplitudeQ_I  an amplitude unbalance introduced to the Q channel to
    // compensate for errors in external hardware..
    void amplitudeQI(float32_t _a) {
        amplitudeQ_I = _a;
        return;
    }

    // The amplitude, a, is the peak, as in zero-to-peak.  This produces outputs
    // ranging from -a to +a.  Both outputs are the same amplitude.
    // This will be multiplied by the AM input from Input 0.  This is "power control"
    void amplitude(float32_t _a) {
        amplitude_pk = _a;
        return;
        }

    void doModulation_AM_PM_FM(bool _doAM, bool _doPM, bool _doFM, bool _bothIQ)  {
        doAM = _doAM;
        doPM = _doPM;
        doFM = _doFM;
        if(doPM & doFM)  doFM=false;  // One at a time
        bothIQ = _bothIQ;
        }

    // Do not use.  For now, dynamic sample rate is not generally supported.
    void setSampleRate_Hz(float32_t fs_Hz) {
		sample_rate_Hz = fs_Hz;
        // Check freq range
        if (freq > sample_rate_Hz/2.0f) freq = sample_rate_Hz/2.0f;
        // update phase increment for new frequency, and kp
        phaseIncrement0 = 512.0f * freq/fs_Hz;
        kp = 512.0f/sample_rate_Hz;
        }

    // Do not use.  Dynamic block length is un-supported.
    void setBlockLength(uint16_t bl) {
      if(bl > 128)  bl = 128;
      block_length = bl;
      }

    virtual void update(void);

private:
    audio_block_f32_t *inputQueueArray_f32[2];
    float32_t freq = 10000.0f;  // Center frequecy, Hz
    float32_t deviationFMScale = 1.0f; // Hz is default, 1000.0 is kHz, not Hz
    float32_t phaseS = 0.0f;
    float32_t phaseQ_I = 128.00;
    float32_t amplitudeQ_I = 1.0f;
    float32_t amplitude_pk = 1.0f;
    float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;  // Base, center freq
    float32_t kp = 512.0/sample_rate_Hz;
    float32_t phaseIncrement0 = kp*freq;;
    uint16_t  block_length = 128;
    bool      doAM = false;
    bool      doPM = false;
    bool      doFM = false;
    bool      bothIQ = false;  // Quadrature outputs for analog mixers
};
#endif
