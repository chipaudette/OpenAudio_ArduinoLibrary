/**
  ******************************************************************************
  * @file    AudioFilterConvolution_F32.cpp
  * @author  Giuseppe Callipo - IK8YFW - ik8yfw@libero.it
  * @version V2.0.0
  * @date    06-02-2021
  * @brief   F32 Filter Convolution
  *
  ******************************************************************************
  ******************************************************************************
   This software is based on the  AudioFilterConvolution routine
   Written by Brian Millier on Mar 2017

   https://circuitcellar.com/research-design-hub/fancy-filtering-with-the-teensy-3-6/
   and modified by Giuseppe Callipo - ik8yfw.
   Modifications:
    1) Class refactoring, change some methods visibility;
    2) Filter coefficients calculation included into class;
    3) Change the class for running in both with F32
       OpenAudio_ArduinoLibrary for Teensy;
    4) Added initFilter method for single anf fast initialization and on
       the fly reinititializzation;
    5) Optimize it to use as output audio filter on SDR receiver.
    6) Optimize the time execution
  *******************************************************************/

  /*    Additional Notes from Bob
   * Object creations is required.  See the OpenAudio_ArduinoLibrary
   * Design Tool for object declarations along with
   * automatic generatin of code.  As an example this could produce
   * the following needed global code
   *   AudioFilterConvolution_F32  FilterConv(audio_settings);
   *   AudioConnection_F32         patchCord1(FilterConv,0,Output_i2s,0);
   *   AudioConnection_F32         patchCord2(FilterConv,0,Output_i2s,1);
   *
   * There are three class functions:
   *    void initFilter(float32_t fc, float32_t Astop,
   *                     int type, float32_t dfc);
   *    void passThrough(int stat);
   *    float32_t* getCoeffPtr(void);
   *
   * initFilter() is used to design the "mask" function that sets the filter
   * response.  All filters use the Kaiser window that is characterized by
   * a programable first sidelobe level and decreasing sidelobes as the
   * frequency departs from the pass band.  For many applications this is an
   * excellent response.  The response type is set by the integer "type."  The
   * options are:
   *   type=LOWPASS   Low pass with fc cutoff frequency and dfc not used.
   *   type=HIGHPASS   High pass with fc cutoff frequency and dfc not used.
   *   type=BANDPASS   Band pass with fc center frequency and dfc pass band width.
   *   type=BANDREJECT Band reject with fc center frequency and dfc reject band width.
   *   type=HILBERT    Hilbert transform.   *** Not Currently Available ***
   *
   * Astop is a value in dB that approximates the first sidelobe level
   * going into the stop band.  This is a feature of the Kaiser window that
   * allows trading off first sidelobe levels against the speed of
   * transition from the passband to the stop band(s).  Values in the 25
   * to 70 dB range work well.
   *
   * Two examples of initFilter():
   *   // IK8YFW CW - Centered at 800Hz, ( 40 db x oct ), 2=BPF, width = 1200Hz
   *   FilterConv.initFilter((float32_t)800, 40, 2, 1200.0);
   *
   *   // IK8YFWSSB - Centered at 1500Hz, ( 60 db x oct ), 2=BPF, width = 3000Hz
   *   FilterConv.initFilter((float32_t)1500, 60, 2, 3000.0);
   *
   * The band edges of filters here are specified by their -6 dB points.
   *
   * passThrough(int stat) allows data for this filter object to be passed through
   * unchanged with stat=1. The dfault is stat=0.
   *
   * getCoeffPtr() returns a pointer to the coefficient array. To use this, compute
   * the coefficients of a 512 tap FIR filter with the desired response.  Then
   * load the 512 float32_t buffer with the coefficients.  Disabling the audio
   * path may be needed to prevent "pop" noises.
   *
   * An alternate way to specify
   *
   * This class is compatible with, and included in, OpenAudio_ArduinoLibrary_F32.
   * If you are using the include OpenAudio_ArduinoLibrary.h, this class's
   * include file will be swept in.
   *
   * Only block_size = 128 is supported.
   * Sample rate can be changed.
   *
   * Speed of execution is the force behind the convolution filter form.
   * Measured 128 sample in update() is 139 microseconds (T4.x).
   * Comparison with a conventional FIR from this library, the
   * AudioFilterFIRGeneral_F32, showed that a 512 tap FIR gave
   * essentially the same response but was somewhat slower at
   * 225 microseconds per 128 update.  Also, note that this form of the
   * computation uses about 44 kB of data memory where the direct FIR
   * uses about 10 kB.
   *
   * See the example TestConvolutionFilter.ino for more inforation on the
   * use of this class.
   *
   * NOTE: This filter can be run under Teensy 3.5, 3.6, 4.0, 4.1 ONLY
   *
   * Removed #defines that were not needed. Thanks K7MDL. Bob 6 Mar 2022
   * Separated Teensy 3 and 4 parts. Thanks Paul  Bob  16 Jan 2023
   *
   * ************************************************************ */
// Only exists for T3.5 through T4.1:
#if defined(__MK64FX512__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)

#ifndef AudioFilterConvolution_F32_h_
#define AudioFilterConvolution_F32_h_

#include <AudioStream_F32.h>
#include "arm_math.h"
#include "arm_common_tables.h"

#if defined(__IMXRT1062__)
#include "arm_const_structs.h"
#endif

#define MAX_NUMCOEF 513

#define PIH_F32    1.5707963f

#define LOWPASS    0
#define HIGHPASS   1
#define BANDPASS   2
#define BANDREJECT 3
#define HILBERT    4

class AudioFilterConvolution_F32 :
 public AudioStream_F32
{
public:
  AudioFilterConvolution_F32(void) :
            AudioStream_F32(1, inputQueueArray_F32)
        {
        fs = AUDIO_SAMPLE_RATE;
        //block_size = 128;  // Always
        // INFO: __MK20DX128__ T_LC;  __MKL26Z64__ T3.0;  __MK20DX256__ T3.1 and T3.2
        //       __MK64FX512__) T3.5; __MK66FX1M0__ T3.6; __IMXRT1062__ T4.0 and T4.1
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
        arm_cfft_radix4_init_f32(&fft_instFwd, 1024, 0, 1);
        arm_cfft_radix4_init_f32(&fft_instRev, 1024, 1, 1);
        // arm CMSIS library has predefined structures of type arm_cfft_instance_f32
        // arm_cfft_sR_f32_len1024 is one of the structures
#endif
        };

  AudioFilterConvolution_F32(const AudioSettings_F32 &settings) :
            AudioStream_F32(1, inputQueueArray_F32)
        {
        // Performs the first initialize
        fs = settings.sample_rate_Hz;
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
        arm_cfft_radix4_init_f32(&fft_instFwd, 1024, 0, 1);
        arm_cfft_radix4_init_f32(&fft_instRev, 1024, 1, 1);
#endif
        };

  virtual void update(void);
  void passThrough(int stat);
  void initFilter (void) {impulse(FIR_Coef);}
  void initFilter (float32_t fc, float32_t Astop,
                   int type, float32_t dfc);
  float32_t* getCoeffPtr(void) {return &FIR_Coef[0];}

private:
  float32_t fs;
  audio_block_f32_t *inputQueueArray_F32[1];
  float32_t *sp_L;
  volatile uint8_t state;
  int i;
  int k;
  int l;
  int passThru=0;
  int enabled=0;
  float32_t FIR_Coef[MAX_NUMCOEF];
  const uint32_t FFT_length = 1024;
  float32_t FIR_filter_mask[2048] __attribute__((aligned(4)));
  float32_t buffer[2048] __attribute__((aligned(4)));
  float32_t tbuffer[2048]__attribute__((aligned(4)));
  float32_t FFT_buffer[2048] __attribute__((aligned(4)));
  float32_t iFFT_buffer[2048] __attribute__((aligned(4)));
  float32_t last_sample_buffer_L[512];
  void impulse(float32_t *coefs);
                                                           int aaa = 0;
  float32_t Izero (float32_t x);
  float32_t m_sinc(int m, float32_t fc);
  void      calc_FIR_coeffs (float32_t * coeffs, int numCoeffs,
                             float32_t fc, float32_t Astop,
                             int type, float32_t dfc,
                             float32_t Fsamprate);

#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
  arm_cfft_radix4_instance_f32 fft_instFwd, fft_instRev;
  // #elif defined(__IMXRT1062__)
  // arm_cfft_instance_f32  arm_cfft_sR_f32_len1024 is built into cmsis
#endif
};

// end of read only once
#endif

// End T3.5, T3.6 or T4.x
#endif
