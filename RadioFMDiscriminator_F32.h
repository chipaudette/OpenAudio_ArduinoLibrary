/*
 * RadioFMDiscriminator_F32
 * 25 April 2022     Bob Larkin
 * With much credit to:
 *    Chip Audette (OpenAudio) Feb 2017
 *    Building from AudioFilterFIR from Teensy Audio Library
 *    (AudioFilterFIR credited to Pete (El Supremo))
 *    and of course, to PJRC for the Teensy and Teensy Audio Library
 *
 * Copyright (c) 2022 Bob Larkin
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
 * frequency of the input sine wave frequency, i.e., it is an FM discriminator.
 *
 * NOTE: Due to the sample frequencies we are working with, like 44.1 kHz, this
 * discriminator cannot handle full FM broadcast bandwidths.  It is suitable for
 * NBFM as used in communications, marine radio, ham radio, etc.
 *
 * The output can be FIR filtered using default parameters,
 * or using coefficients from an array. A separate single pole de-emphasis filer
 * is included that again can be programmed.
 *
 * Internally, the discriminator uses a pair of single pole BPF that
 *
 * Status:
 *
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
 * Time:     For T4.0,  45 microseconds for a block of 128 data points.
 *
 */

#ifndef _radioFMDiscriminator_f32_h
#define _radioFMDiscriminator_f32_h

//#include "mathDSP_F32.h"
#include "AudioStream_F32.h"
//#include "arm_math.h"

#define LPF_NONE 0
#define LPF_FIR 1
#define LPF_IIR 2

#define TEST_TIME_FM 0

class RadioFMDiscriminator_F32 : public AudioStream_F32 {
//GUI: inputs:1, outputs:2  //this line used for automatic generation of GUI node
//GUI: shortName: FMDiscriminator
public:
   // Default block size and sample rate:
   RadioFMDiscriminator_F32(void) :  AudioStream_F32(1, inputQueueArray_f32) {
   }
   // Option of AudioSettings_F32 change to block size and/or sample rate:
   RadioFMDiscriminator_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) {
      sampleRate_Hz = settings.sample_rate_Hz;
      block_size = settings.audio_block_samples;
   }

   // This sets the parameters of the discriminator.  The output LPF, if any,
   // must precede this function.
   void initializeFMDiscriminator(float32_t _f1, float32_t _f2, float32_t _q1, float32_t _q2) {
      f1 = _f1;  f2 = _f2;
      q1 = _q1;  q2 = _q2;

      // Design the 2 single pole filters:
      setBandpass(coeff_f1BPF, f1, q1);
      setBandpass(coeff_f2BPF, f2, q2);
      // Initialize BiQuad instances for BPF's (ARM DSP Math Library)
      // https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html
      // arm_biquad_cascade_df1_init_f32(&biquad_inst, numStagesUsed, &coeff32[0],  &StateF32[0])
      arm_biquad_cascade_df1_init_f32(&f1BPF_inst, 1, &coeff_f1BPF[0],  &state_f1BPF[0]);
      arm_biquad_cascade_df1_init_f32(&f2BPF_inst, 1, &coeff_f2BPF[0],  &state_f2BPF[0]);

      /* The FIR instance setup call
       *   void  arm_fir_init_f32(
       *   arm_fir_instance_f32* S,       points to instance of floating-point FIR filter struct
       *   uint16_t              numTaps, Number of filter coefficients in the filter.
       *   float32_t*            pCoeffs, points to the filter coefficients buffer.
       *   float32_t*            pState,  points to the state buffer.
       *   uint32_t              blockSize) Number of samples that are processed per call.
       */
      if (fir_Out_Coeffs  && outputFilterType == LPF_FIR)
         {
         arm_fir_init_f32(&FMDet_Out_inst, nFIR_Out, &fir_Out_Coeffs[0],
            &State_FIR_Out[0], (uint32_t)block_size);
         }
      else
         {
            ;
         }

      // Initialize squelch Input BPF BiQuad instance
      arm_biquad_cascade_df1_init_f32(&iirSqIn_inst, 2, pCfSq,  &stateSqIn[0]);
   }

   // Provide for changing to user FIR for discriminator output, (and user de-emphasis)
   // This should precede setting discriminator parameters
   void filterOutFIR(float32_t *_fir_Out_Coeffs, int _nFIR_Out, float32_t *_State_FIR_Out, float32_t _Kdem) {
      if(_fir_Out_Coeffs==NULL)
         {
         outputFilterType = LPF_NONE;
         return;
         }
      if( _Kdem<0.0001 || _Kdem>1.0 ) {
         return;
         }
      outputFilterType = LPF_FIR;
      fir_Out_Coeffs = _fir_Out_Coeffs;
      nFIR_Out = _nFIR_Out;
      State_FIR_Out = _State_FIR_Out;
      Kdem = _Kdem;
      OneMinusKdem = 1.0f - Kdem;
   }

   // This should precede setting discriminator parameters, if used
   void filterOutIIR(float32_t _frequency, float32_t _q, float32_t _Kdem) {
      if( _frequency < 0.0001f)
         {
         outputFilterType = LPF_NONE;
         return;
         }
      outputFilterType = LPF_IIR;
      setLowpass(coeff_outLPF, _frequency, _q);
      arm_biquad_cascade_df1_init_f32(&outLPF_inst, 1, &coeff_outLPF[0],  &state_outLPF[0]);

      if( _Kdem<0.0001 || _Kdem>1.0 ) {
         return;
         }
      Kdem = _Kdem;
      OneMinusKdem = 1.0f - Kdem;
   }

   // Provide for changing to user supplied BiQuad for Squelch input.
   // This should precede setting discriminator parameters, if used
   void setSquelchFilter(float* _sqCoeffs)  {
      if( _sqCoeffs==NULL)
         pCfSq = coeffSqIn;   // Default filter
      else
         pCfSq = _sqCoeffs;
      }

   // The squelch level reads nominally 0.0 to 1.0 where
   float getSquelchLevel (void)  {
      return squelchLevel;
      }

   // The squelch threshold is nominally 0.7 where
   // 0.0 always lets audio through.
   void setSquelchThreshold (float _sqTh)  {
      squelchThreshold = _sqTh;
      }

   void setSquelchDecay (float _sqDcy)  {
      gamma = _sqDcy;
      alpha = 0.5f*(1.0f - gamma);
      }

   // This should precede setting discriminator parameters, if used
   void setSampleRate_Hz(float32_t _sampleRate_Hz) {
      sampleRate_Hz = _sampleRate_Hz;
      }

   virtual void update(void);

private:
   // One input data pointer
   audio_block_f32_t *inputQueueArray_f32[1];
   float32_t sampleRate_Hz = AUDIO_SAMPLE_RATE_EXACT;
   uint16_t block_size = AUDIO_BLOCK_SAMPLES;

   /* A pair of single pole BPF for the discriminator:
    * Info - The structure from arm_biquad_casd_df1_inst_f32 consists of
    *    uint32_t  numStages;
    *    const float32_t *pCoeffs;  //Points to the array of coefficients, length 5*numStages.
    *    float32_t *pState;         //Points to the array of state variables, length 4*numStages.
    */
   float f1, q1, f2, q2;
   arm_biquad_casd_df1_inst_f32 f1BPF_inst;
   float coeff_f1BPF[5];
   float state_f1BPF[4];
   arm_biquad_casd_df1_inst_f32 f2BPF_inst;
   float coeff_f2BPF[5];
   float state_f2BPF[4];

   // De-emphasis constant
   float32_t   Kdem =         0.045334f;
   float32_t   OneMinusKdem = 0.954666f;
   // Save last data point for next update of de-emphasis filter
   float32_t dLast = -1.0f;

   // The output FIR LPF (optional)
   int outputFilterType = LPF_NONE;
   // ARM CMSIS FIR filter instances and State vectors
   arm_fir_instance_f32 FMDet_Out_inst;
   float32_t *State_FIR_Out;    // 128+nFIR_Out
   uint16_t nFIR_Out;
   float32_t* fir_Out_Coeffs = NULL;
   float32_t discrOut = 0.0f;
   // Output IIR Biquad alternative
   arm_biquad_casd_df1_inst_f32 outLPF_inst;
   float coeff_outLPF[5];
   float state_outLPF[4];

   arm_biquad_casd_df1_inst_f32 iirSqIn_inst;
   // Default 2 stage Squelch input BiQuad filter, 3000 Hz, 4000 Hz both Q=5
   // The -6 dB points are  2680 and 4420 Hz
   // The -20 dB points are 2300 and 5300 Hz
   float  coeffSqIn[10] = {
      0.0398031529f, 0.0f, -0.0398031529f, 1.74762569f, -0.92039369f,
      0.0511929547f, 0.0f, -0.0511929547f, 1.59770204f, -0.89761409f};
   float* pCfSq = coeffSqIn;
   float stateSqIn[8];
   float squelchThreshold = 0.7f;
   float squelchLevel = 1.0f;
   float gamma = 0.99;
   float alpha = 0.5f*(1.0f - gamma);

#if TEST_TIME_FM
elapsedMicros tElapse;
int32_t iitt = 999000;     // count up to a million during startup
#endif

#if 0
   /* Info Only, an example FIR filter, include this in INO to use.
    * FIR filter designed with http://t-filter.appspot.com
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
#endif

   // Unity gain BPF Biquad, CMSIS format (not Matlab)
   void setBandpass(float32_t* pCoeff, float32_t frequency, float32_t q) {
      float32_t w0 = 2.0f*3.141592654f*frequency/sampleRate_Hz;
      float32_t alpha = sin(w0)/(2.0f*q);
      float32_t scale = 1.0f/(1.0f + alpha);
      /* b0 */ *(pCoeff+0) = alpha*scale;
      /* b1 */ *(pCoeff+1) = 0.0f;
      /* b2 */ *(pCoeff+2) = (-alpha)*scale;
      /* a1 */ *(pCoeff+3) = -(-2.0f*cos(w0))*scale;
      /* a2 */ *(pCoeff+4) = -(1.0f - alpha)*scale;
      }

   // Unity gain LPF, CMSIS format
   void setLowpass(float32_t* pCoeff, float32_t frequency, float32_t q) {
      float32_t w0 = frequency*(2.0f*3.141592654f / sampleRate_Hz);
      float32_t alpha = sin(w0) / ((double)q*2.0f);
      float32_t cosW0 = cos(w0);
      float32_t scale = 1.0f/(1.0f+alpha); // which is equal to 1.0f / a0
      /* b0 */ *(pCoeff+0) = ((1.0f - cosW0) / 2.0f)*scale;
      /* b1 */ *(pCoeff+1) = (1.0f - cosW0)*scale;
      /* b2 */ *(pCoeff+2) = *(pCoeff+0);
      /* a1 */ *(pCoeff+3) = -(-2.0f*cosW0)*scale;
      /* a2 */ *(pCoeff+4) = -(1.0f - alpha)*scale;
      }
   };
#endif
