/*
 * RadioFMDetector_F32
 * 22 March 2020     Bob Larkin
 * With much credit to:
 *    Chip Audette (OpenAudio) Feb 2017
 *    Building from AudioFilterFIR from Teensy Audio Library
 *    (AudioFilterFIR credited to Pete (El Supremo))
 *    and of course, to PJRC for the Teensy and Teensy Audio Library
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

/* This consists of a single input at some frequency, such as 10 to 20 kHz and
 * an output, such as 0 to 5 kHz.  The output level is linearly dependent on the
 * frequency of the input sine wave frequency, i.e., an it is an FM detector.
 * The input needs to be band limited below the lower frequency side of the
 * input, typically 10 kHz.  This is not part of this block.
 * 
 * NOTE: Due to the sample frequencies we are working with, like 44.1 kHz, this
 * detector cannot handle full FM broadcast bandwidths.  It is suitable for
 * NBFM as used in communications, marine radio, ham radio, etc.
 *
 * The output can be FIR filtered using default parameters,
 * or using coefficients from an array. A separate single pole de-emphasis filer
 * is included that again can be programmed.
 *
 * Internally, the detector uses a pair of mixers (multipliers) that generate the
 * in-phase and quadrature inputs to a atan2 type of phase detector.  These
 * mixers have two output signals at the difference (desired) and sum (undesired)
 * frequencies.  The high frequency sum signal can be filtered (For a 15 kHz center,
 * with an input band of 10 to 20 kHz the sum signal will be from 25 to 35 kHz that
 * wraps around the 22 kHz half-sample point to produce 19 to 9 kHz. This needs to
 * be removed before the atan2.  A pair of FIR filters, using FIR_IQ_Coeffs
 * are used.  These are again programmable and default to a 29-tap LPF with
 * a 5 kHz cutoff.
 * 
 * Status: Tested static, tested with FM modulated Fluke 6061B.
 *         An input of about 60 microvolts to the SGTL5000 gave 12 dB SINAD.
 *         The output sounded good. Tested T3.6 and T4.0. No known bugs
 * 
 * Output: Float, sensitivity is 2*pi*(f - fCenter)*sample_rate_Hz
 *         For 44117Hz samplerate, this is 0.000142421 per Hz
 * 
 * Accuracy:  The function used is precise.  However, the approximations, such
 *            fastAtan2, slightly limit the accuracy.  A 200 point sample of a
 *            14 kHz input had an average error of 0.03 Hz 
 *            and a standard deviation of 0.81 Hz.
 *            The largest errors in this sample were about +/- 1.7 Hz.  This is
 *            with the default filters.
 *
 * Functions:
 *   frequency(float fCenter ) sets the center frequency in Hz, default 15000.
 *
 *   filterOut(float *firCoeffs, uint nFIR, float Kdem) sets output filtering where:
 *      float32_t*  firCoeffs is an array of coefficients
 *      uint        nFIR is the number of coefficients
 *      float32_t   Kdem is the de-emphasis frequency factor, where
 *                  Kdem = 1/(0.5+(tau*fsample))  and tau is the de-emphasis
 *                  time constant, typically 0.0005 second and fsample is
 *                  the sample frequency, typically 44117.
 *
 *   filterIQ(float *fir_IQ_Coeffs, uint nFIR_IQ) sets output filtering where:
 *      float32_t*  fir_IQ_Coeffs is an array of coefficients
 *      uint        nFIR_IQ is the number of coefficients, max 60
 * 
 *   setSampleRate_Hz(float32_t _sampleRate_Hz) allows dynamic changing of
 *                  the sample rate (experimental as of May 2020).
 * 
 *   returnInitializeFMError()  Returns the initialization errors.
 *                  B0001 (value 1) is an error in the IQ FIR Coefficients or quantity.
 *                  B0010 (value 2) is an error in the Output FIR Coefficients or quantity.
 *                  B0100 (value 4) is an error in the de-emphasis constant
 *                  B1000 (value 8) is center frequency above half-sample frequency.
 *                  All for debug.
 * 
 *   showError(uint16_t e) Turns error printing in the update function on (e=1)
 *                  or off (e=0).  For debug only.
 *
 * Time:            For T3.6, an update of a 128 sample block, 370 microseconds, or
 *                  2.9 microseconds per data point.
 *                  For T4.0, 87 microseconds, or 0.68 microseconds per data point.
 * 
 * Error checking:  See functions setSampleRate_Hz() and returnInitializeFMError()
 *                  above.
 */

#ifndef _radioFMDetector_f32_h
#define _radioFMDetector_f32_h

#include "mathDSP_F32.h"
#include "AudioStream_F32.h"
#include "arm_math.h"

#define MAX_FIR_IQ_COEFFS  100
#define MAX_FIR_OUT_COEFFS 120

#define TEST_TIME_FM 0

class RadioFMDetector_F32 : public AudioStream_F32 {
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName: FMDetector
public:
    // Default block size and sample rate:
    RadioFMDetector_F32(void) :  AudioStream_F32(1, inputQueueArray_f32) {
       initializeFM();
    }
    // Option of AudioSettings_F32 change to block size and/or sample rate:
    RadioFMDetector_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) {
        sampleRate_Hz = settings.sample_rate_Hz;
        block_size = settings.audio_block_samples;
        initializeFM();
    }

    // Provide for changing input center frequency, in Hz
    void frequency(float32_t _fCenter) {
        fCenter = _fCenter;
        phaseIncrement = 512.0f * fCenter / sampleRate_Hz;
    }

    // Provide for user FIR for I and Q signals to user supplied array
    void filterIQ(float32_t* _fir_IQ_Coeffs, int _nFIR_IQ) {
        if( fir_IQ_Coeffs==NULL || nFIR_IQ<4 || nFIR_IQ>MAX_FIR_IQ_COEFFS ) {
            initializeFMErrors |= 1;
            return;
        }
        fir_IQ_Coeffs = _fir_IQ_Coeffs;
        nFIR_IQ = _nFIR_IQ;
        initializeFM();
    }

    // Provide for changing to user FIR for detector output, (and user de-emphasis)
    void filterOut(float32_t *_fir_Out_Coeffs, int _nFIR_Out, float32_t _Kdem) {
        if( _fir_Out_Coeffs==NULL || _nFIR_Out<4 || _nFIR_Out>MAX_FIR_OUT_COEFFS) {
            initializeFMErrors |= 2;
            return;
        }
        if( _Kdem<0.0001 || _Kdem>1.0 ) {
            initializeFMErrors |= 4;
            return;
        }
        fir_Out_Coeffs = _fir_Out_Coeffs;
        nFIR_Out = _nFIR_Out;
        Kdem = _Kdem;
        OneMinusKdem = 1.0f - Kdem;
        initializeFM();
    }

    void setSampleRate_Hz(float32_t _sampleRate_Hz) {
        if (fCenter > _sampleRate_Hz/2.0f) {   // Check freq range
            initializeFMErrors |= 8;
            return;
        }
        sampleRate_Hz = _sampleRate_Hz;
        // update phase increment for new frequency
        phaseIncrement = 512.0f * fCenter / sampleRate_Hz;
    }

    void showError(uint16_t e) {
        errorPrintFM = e;
    }
    
    uint16_t returnInitializeFMError(void) {
		return initializeFMErrors;
	}

    void update(void);
    
private:
    // One input data pointer
    audio_block_f32_t *inputQueueArray_f32[1];
    float32_t fCenter = 15000.0f;
    float32_t phaseS = 0.0f;
    float32_t phaseS_C = 128.00f;
    float32_t phaseIncrement = 512.0f*15000.0f/AUDIO_SAMPLE_RATE_EXACT;
    float32_t sampleRate_Hz = AUDIO_SAMPLE_RATE_EXACT;
    uint16_t block_size = AUDIO_BLOCK_SAMPLES;
    // De-emphasis constant
    float32_t   Kdem =         0.045334f;
    float32_t   OneMinusKdem = 0.954666f;
    // Save last data point of atan2 for differentiator
    float32_t diffLast = 0.0f;
    // Save last data point for next update of de-emphasis filter
    float32_t dLast = 0.0f;
    // Control error printing in update(), normally off
    uint16_t errorPrintFM = 0;
    // Monitor constructor errors
    uint16_t initializeFMErrors = 0;
    uint16_t nFIR_IQ = 29;
    float32_t* fir_IQ_Coeffs = fir_IQ29;
    uint16_t nFIR_Out = 39;
    float32_t* fir_Out_Coeffs = fir_Out39;
#if TEST_TIME_FM
elapsedMicros tElapse;
int32_t iitt = 999000;     // count up to a million during startup
#endif
    // ARM CMSIS FIR filter instances and State vectors, sized for max, max
    arm_fir_instance_f32 FMDet_I_inst;
    float32_t State_I_F32[AUDIO_BLOCK_SAMPLES + MAX_FIR_IQ_COEFFS];  // 228

    arm_fir_instance_f32 FMDet_Q_inst;
    float32_t State_Q_F32[AUDIO_BLOCK_SAMPLES + MAX_FIR_IQ_COEFFS];  // 248

    arm_fir_instance_f32 FMDet_Out_inst;
    float32_t State_Out_F32[AUDIO_BLOCK_SAMPLES + MAX_FIR_OUT_COEFFS];
       
    // Initialize the FM Detector, part of setting up and changing parameters
    void initializeFM(void) {
		if (fir_IQ_Coeffs  && nFIR_IQ <= MAX_FIR_IQ_COEFFS) {
/*          the instance setup call
 *          void  arm_fir_init_f32(          
 *          arm_fir_instance_f32* S,       points to instance of floating-point FIR filter structure.
 *           uint16_t              numTaps, Number of filter coefficients in the filter.
 *           float32_t*            pCoeffs, points to the filter coefficients buffer.
 *           float32_t*            pState,  points to the state buffer.
 *           uint32_t              blockSize) Number of samples that are processed per call.
 */
            arm_fir_init_f32(&FMDet_I_inst, nFIR_IQ, (float32_t*)fir_IQ_Coeffs, &State_I_F32[0], (uint32_t)block_size);   
            arm_fir_init_f32(&FMDet_Q_inst, nFIR_IQ, (float32_t*)fir_IQ_Coeffs, &State_Q_F32[0], (uint32_t)block_size);         
        }
        else  initializeFMErrors |= B0001;     

        if (fir_Out_Coeffs  && nFIR_Out <= MAX_FIR_OUT_COEFFS) {
            arm_fir_init_f32(&FMDet_Out_inst, nFIR_Out, (float32_t*)fir_Out_Coeffs,  &State_Out_F32[0], (uint32_t)block_size);
        }
        else  initializeFMErrors |= B0010;
        dLast = 0.0;
     }

    /* FIR filter designed with http://t-filter.appspot.com
     * fs = 44100 Hz, < 5kHz ripple 0.29 dB, >9 kHz, -62 dB, 29 taps
     */
    float32_t fir_IQ29[29] = {
    -0.000970689f, -0.004690292f, -0.008256345f, -0.007565650f,
     0.001524420f,  0.015435011f,  0.021920240f,  0.008211937f,
    -0.024286413f, -0.052184700f, -0.040532507f,  0.031248107f,
     0.146902412f,  0.255179564f,  0.299445269f,  0.255179564f,
     0.146902412f,  0.031248107f, -0.040532507f, -0.052184700f,
    -0.024286413f,  0.008211937f,  0.021920240f,  0.015435011f,
     0.001524420f, -0.007565650f, -0.008256345f, -0.004690292f,
    -0.000970689f};

    /* FIR filter designed with http://t-filter.appspot.com
     * fs = 44100 Hz, < 3kHz ripple 0.36 dB, >6 kHz, -60 dB, 39 taps
     * Corrected to give DC gain = 1.00 
     */
    float32_t fir_Out39[39] = {
    -0.0008908477f,  -0.0008401274f,  -0.0001837353f,   0.0017556005f, 
     0.0049353322f,   0.0084952916f,   0.0107668722f,   0.0097441685f, 
     0.0039877576f,  -0.0063455016f,  -0.0188069300f,  -0.0287453055f, 
    -0.0303831521f,  -0.0186809770f,   0.0085931270f,   0.0493875744f, 
     0.0971742012f,   0.1423015880f,   0.1745838382f,   0.1863024485f, 
     0.1745838382f,   0.1423015880f,   0.0971742012f,   0.0493875744f, 
     0.0085931270f,  -0.0186809770f,  -0.0303831521f,  -0.0287453055f, 
    -0.0188069300f,  -0.0063455016f,   0.0039877576f,   0.0097441685f, 
     0.0107668722f,   0.0084952916f,   0.0049353322f,   0.0017556005f, 
    -0.0001837353f,  -0.0008401274f,  -0.0008908477f };

};
#endif 
