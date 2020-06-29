/*
 * AudioAnalyzePhase_F32.h
 *
 * 31 March 2020, Rev 8 April 2020  
 * Status Tested OK T3.6 and T4.0. 
 * Bob Larkin, in support of the library:
 * Chip Audette, OpenAudio, Apr 2017
 *     -------------------
 * There are two inputs, 0 and 1 (Left and Right)
 * There is one output, the phase angle between 0 & 1 expressed in
 * radians (180 degrees is Pi radians) or degrees.  This is a 180-degree
 * type of phase detector.  See RadioIQMixer_F32 for a 360 degree type.
 *
 * This block can be used to measure phase between two sinusoids, and the default IIR filter is suitable for this with a cut-off
 * frequency of 100 Hz. The only IIR configuration is 4-cascaded satages of BiQuad.  For this, 20 coefficients must be provided
 * in 4 times (b0, b1, b2, -a1, -a2) order (example below). This IIR filter inherently does not have very good
 * linearity in phase vs. frequency.  This can be a problem for communications systems.
 * As an alternative, a linear phase (as long as coefficients are symmetrical)
 * FIR filter can be set up with the begin method.  The built in FIR LP filter has a cutoff frequency of 4 kHz when used
 * at a 44.1 kHz sample rate.  This filter uses 53 coefficients (called taps).  Any FIR filter with 4 to 200 coefficients can be used
 * as set up by the begin method.
 *
 * DEFAULTS: 100 Hz IIR LP, output is in radians, and this does *NOT* need a call to begin().  This can be changed, including
 * using a FIR LP where linear phase is needed, or NO_LP_FILTER that leaves harmonics of the input frequency.  Method begin()
 * changes the options.  For instance, to use a 60 coefficient FIR the setup() in the .INO might do
 *    myAnalyzePhase.begin(FIR_LP_FILTER, &myFIRCoefficients[0], 60, DEGREES_PHASE);
 * If _pcoefficients is NULL, the coefficients will be left default.  For instance, to use the default 100 Hz IIR filter, with degree output
 *    myAnalyzePhase.begin(IIR_LP_FILTER, NULL, 20, DEGREES_PHASE);
 * To provide a new set of IIR coefficients (note strange coefficient order and negation for a() that CMSIS needs)
 *    myAnalyzePhase.begin(IIR_LP_FILTER, &myIIRCoefficients[0], 20, RADIANS_PHASE);
 * In begin() the pdConfig can be set (see #defines below).  The default is to use no limiter, but to measure the input levels over the 
 * block and use that to scale the multiplier output.  This will cause successive blocks to change slightly in output level due to
 * errors in level measurement, but is other wise fine.  If the limiter is used, the narrow band IIR filter should also be used to
 * prevent artifacts from "beats" between the sample rate and the input frequency.
 * 
 * Three different scaling routines are available following the LP filter.  These deal with the issue that the multiplier type
 * of phase detector produces an output proportional to the cosine of the phase angle between the two input sine waves.
 * If the inputs both have a magnitude ranging from -1.0 to 1.0, the output will be cos(phase difference).  Other values of
 * sine wave will multiply this by the product of the two maximum levels.  The selection of "fast" or "accurate" acos() will
 * make the output approximately the angle, as scaled by UNITS_MASK.  The ACOS_MASK bits in pdConfig, set by begin(), selects the
 * acos used.  Note that if acos function is used, the output range is 0 to pi radians, i.e., 0 to 180 degrees.  "Units" have no
 * effect when acos90 is not being used, as that would make little sense for the (-1,1) output.
 * 
 * Functions:
 *    setAnalyzePhaseConfig(const uint16_t LPType, float32_t *pCoeffs, uint16_t nCoeffs)
 *    setAnalyzePhaseConfig(const uint16_t LPType, float32_t *pCoeffs, uint16_t nCoeffs, uint16_t pdConfig)
 *      are used to chhange the output filter from the IIR default, where:
 *        LPType is  NO_LP_FILTER, IIR_LP_FILTER, FIR_LP_FILTER to select the output filter
 *        pCoeffs is a pointer to filter coefficients, either IIR or FIR 
 *        nCoeffs is the number of filter coefficients
 *        pdConfig is bitwise selection (default 0b1100) of
 *          Bit 0: 0=No Limiter (default)   1=Use limiter
 *          Bit 2 and 1: 00=Use no acos linearizer    01=undefined
 *                       10=Fast, math-continuous acos() (default)    11=Accurate acosf()
 *          Bit 3: 0=No scale of multiplier  1=scale to min-max (default)
 *          Bit 4: 0=Output in degrees    1=Output in radians (default)
 *    showError(uint16_t e) sets whether error printing comes from update (e=1) or not (e=0).
 * 
 * Examples:   AudioTestAnalyzePhase.ino and AudioTestSinCos.ino 
 * 
 * Some measured time data for a 128 size block, Teensy 3.6, parts of update():
 *   Default settings, total time 123 microseconds
 *   Overhead of update(), loading arrays, handling blocks, less than 2 microseconds
 *   Min-max calculation, 23 microseconds
 *   Multiplier DBMixer  8 microseconds
 *   IIR LPF (default filter)  57 microseconds
 *   53-term FIR filter  149 microseconds 
 *   Fast acos_32() linearizer  32 microseconds
 *   Accurate acosf(x) seems to vary (with x?), 150 to 350 microsecond range
 * 
 * Measured total update() time for the min-max scaling, fast acos(), and 53-term FIR filtering
 *      case is 214 microseconds for Teensy 3.6 and 45 microseconds for Teensy 4.0.
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

#ifndef _analyze_phase_f32_h
#define _analyze_phase_f32_h

#define N_STAGES 4
#define NFIR_MAX 200
#define NO_LP_FILTER 0
#define IIR_LP_FILTER 1
#define FIR_LP_FILTER 2
#define RADIANS_PHASE 1.0
#define DEGREES_PHASE 57.295779

// Test the number of microseconds to execute update()
#define TEST_TIME 1

#define LIMITER_MASK 0b00001
#define ACOS_MASK    0b00110
#define SCALE_MASK   0b01000
#define UNITS_MASK   0b10000

#include "AudioStream_F32.h"
#include <math.h>

class AudioAnalyzePhase_F32 : public AudioStream_F32 {
//GUI: inputs:2, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName: AnalyzePhase
public:
    // Option of AudioSettings_F32 change to block size or sample rate:
    AudioAnalyzePhase_F32(void) :  AudioStream_F32(2, inputQueueArray_f32) { // default block_size and sampleRate_Hz
	// Initialize BiQuad IIR instance (ARM DSP Math Library)
        arm_biquad_cascade_df1_init_f32(&iir_inst, N_STAGES, &iir_coeffs[0],  &IIRStateF32[0]);
    }
    // Constructor including new block_size and/or sampleRate_Hz
    AudioAnalyzePhase_F32(const AudioSettings_F32 &settings) :  AudioStream_F32(2, inputQueueArray_f32) {
        block_size = settings.audio_block_samples;
	    sampleRate_Hz = settings.sample_rate_Hz;        
	// Initialize BiQuad IIR instance (ARM DSP Math Library)
        arm_biquad_cascade_df1_init_f32(&iir_inst, N_STAGES, &iir_coeffs[0],  &IIRStateF32[0]);
    }

    // Set AnalyzePhaseConfig while leaving pdConfig as is
    void setAnalyzePhaseConfig(const uint16_t _LPType, float32_t *_pCoeffs, uint16_t _nCoeffs) {
		 setAnalyzePhaseConfig(               _LPType,            _pCoeffs,          _nCoeffs,  pdConfig);
	}
    // Set AnalyzePhaseConfig in full generality
    void setAnalyzePhaseConfig(const uint16_t _LPType, float32_t *_pCoeffs, uint16_t _nCoeffs, uint16_t _pdConfig) {
        AudioNoInterrupts(); // No interrupts while changing parameters
        LPType = _LPType;
        if (LPType == NO_LP_FILTER) {
            //Serial.println("Advice: in AnalyzePhase, for NO_LP_FILTER the output contains 2nd harmonics");
            //Serial.println("    that need external filtering.");
        }
        else if (LPType == IIR_LP_FILTER) {
            if(_pCoeffs != NULL){
                pIirCoeffs = _pCoeffs;
                nIirCoeffs = _nCoeffs;
            }
            if (nIirCoeffs != 20){
                //Serial.println("Error, in AnalyzePhase, for IIR_LP_FILTER there must be 20 coefficients.");
                nIirCoeffs = 20;
            }
            arm_biquad_cascade_df1_init_f32(&iir_inst, N_STAGES, pIirCoeffs,  &IIRStateF32[0]);
         }   
         else if (LPType==FIR_LP_FILTER) {
            if(_pCoeffs != NULL){
                pFirCoeffs = _pCoeffs;
                nFirCoeffs = _nCoeffs;
            }
			if (nFirCoeffs<4 || nFirCoeffs>NFIR_MAX) {  // Too many or too few
                //Serial.print("Error, in AnalyzePhase, for FIR_LP_FILTER there must be >4 and <=");
                //Serial.print(NFIR_MAX);
                //Serial.println(" coefficients.");
                //Serial.println("    Restoring default IIR Filter.");
                LPType = IIR_LP_FILTER;
                pIirCoeffs = &iir_coeffs[0];
                nIirCoeffs = 20;                 // Number of coefficients 20
                pdConfig = 0b11100;
                LPType = IIR_LP_FILTER;    // Variables were set in setup() above
            }
            else {     //Acceptable number, so initialize it
                arm_fir_init_f32(&fir_inst, nFirCoeffs, pFirCoeffs,  &FIRStateF32[0], block_size);
		    }
        }
        pdConfig = _pdConfig;
        AudioInterrupts(); 
    }

    void showError(uint16_t e) {
        errorPrint = e;
    }

    void update(void);

private:
    float32_t sampleRate_Hz = AUDIO_SAMPLE_RATE_EXACT;
    uint16_t block_size = AUDIO_BLOCK_SAMPLES;
    // Two input data pointers
    audio_block_f32_t *inputQueueArray_f32[2];
    // Variables controlling the configuration
    uint16_t LPType = IIR_LP_FILTER;          // NO_LP_FILTER, IIR_LP_FILTER or FIR_LP_FILTER
    float32_t *pIirCoeffs = &iir_coeffs[0];   // Coefficients for IIR
    float32_t *pFirCoeffs = &fir_coeffs[0];   // Coefficients for FIR
    uint16_t nIirCoeffs = 20;                 // Number of coefficients 20
    uint16_t nFirCoeffs = 53;                 // Number of coefficients <=200
    uint16_t  pdConfig = 0b11100;             // No limiter, fast acos, scale multiplier, radians out;
    // Control error printing in update().  Should never be enabled
    // until all audio objects have been initialized.
    // Only used as 0 or 1 now, but 16 bits are available.
    uint16_t errorPrint = 0;
    
    //  *Temporary* - TEST_TIME allows measuring time in microseconds for each part of the update()
#if TEST_TIME
    elapsedMicros tElapse;
    int32_t iitt = 998000;     // count up to a million during startup
#endif

    /* FIR filter designed with http://t-filter.appspot.com
     * Sampling frequency: 44100 Hz
     * 0 Hz - 4000 Hz   gain = 1.0, ripple = 0.101 dB
     * 7000 - 22000 Hz  attenuation >= 81.8 dB
     * Suitable for measuring phase in communications systems with linear phase.
     */
    float32_t fir_coeffs[53] = {
    -0.000206064,-0.000525129,-0.00083518, -0.000774011,  2.5925E-05,
     0.001614912, 0.003431897, 0.004335125, 0.003127158, -0.000566047,
    -0.005566484,-0.009192163,-0.008417443,-0.001801824,  0.008839149,
     0.018273049, 0.019879265, 0.009349346,-0.011696836, -0.034389317,
    -0.045008839,-0.030706279, 0.013824834, 0.082060266,  0.156328996,
     0.213799940, 0.235420817, 0.213799940, 0.156328996,  0.082060266,
     0.013824834,-0.030706279,-0.045008839,-0.034389317, -0.011696836,
     0.009349346, 0.019879265, 0.018273049, 0.008839149, -0.001801824,
    -0.008417443,-0.009192163,-0.005566484,-0.000566047,  0.003127158,
     0.004335125, 0.003431897, 0.001614912, 2.5925E-05,  -0.000774011,
    -0.000835180,-0.000525129,-0.000206064 };

    // 8-pole Biquad fc=0.0025fs, -80 dB Iowa Hills
    // This is roughly the narrowest that doesn't have
    // artifacts from numerical errors more than about
    // 0.001 radians (0.06 deg), per experiments using F32.
    // b0,b1,b2,a1,a2 for each BiQuad.  Start with stage 0
    float32_t iir_coeffs[5 * N_STAGES]={
    0.08686551007982608,
    -0.1737214710369926,
    0.08686551007982608,
    1.9951804375779567,
    -0.9951899867006161,
    // and stage 1
    0.20909791845765324,
    -0.4181667739705088,
    0.20909791845765324,
    1.9965910753714984,
    -0.9966201383162961,
    // stage 2
    0.18360046797931723,
    -0.3671514768697197,
    0.18360046797931723,
    1.9981966389027592,
    -0.998246097991674,
    // stage 3
    0.03079484444321144,
    -0.061529427044071175,
    0.03079484444321144,
    1.999421284937329,
    -0.9994815467796806};

    // ARM DSP Math library IIR filter instance
    arm_biquad_casd_df1_inst_f32 iir_inst;

    // And a FIR type, as either can be used via begin()
    arm_fir_instance_f32 fir_inst;

    // Delay line space for the FIR
    float32_t FIRStateF32[AUDIO_BLOCK_SAMPLES + NFIR_MAX];

    // Delay line space for the Biquad, each arranged as {x[n-1], x[n-2], y[n-1], y[n-2]}
    float32_t IIRStateF32[4 * N_STAGES];
};
#endif
