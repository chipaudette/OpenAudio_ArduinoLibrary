/*    analyze_fft256_iq_F32.h  Assembled by Bob Larkin   6 Mar 2021
 *
 * Rev 6 Mar 2021 - Added setXAxis()
 * Rev 7 Mar 2021 - Corrected bug in applying windowing
 * Rev 10 Mar 2021 - Corrrected: dBFS offset (up 12 dB) & xAxis for dBFS
 * 
 * Does Fast Fourier Transform of a 256 point complex (I-Q) input.
 * Output is one of three measures of the power in each of the 256
 * output bins, Power, RMS level or dB relative to a full scale
 * sine wave.  Windowing of the input data is provided for to reduce
 * spreading of the power in the output bins.  All inputs are Teensy
 * floating point extension (_F32) and all outputs are floating point.
 *
 * Features include:
 *   * I and Q inputs are OpenAudio_Arduino Library F32 compatible.
 *   * FFT output for every 128 inputs to overlapped FFTs to
 *     compensate for windowing.
 *   * Windowing None, Hann, Kaiser and Blackman-Harris.
 *   * Multiple bin-sum output to simulate wider bins.
 *   * Power averaging of multiple FFT
 *   * Programmable frequency scale arrangement.
 *   * Soon: F32 audio outputs for I & Q
 *
 * Conversion Copyright (c) 2021 Bob Larkin
 * Same MIT license as PJRC:
 *
 *  Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Does complex input FFT of 256 points.  Multiple non-audio (via functions)
 * output formats of RMS (same as I16 version, and default),
 * Power or dBFS (full scale).  Output can be bin by bin or a pointer to
 * the output array is available.  Several window functions are provided by
 * in-class design, or a custom window can be provided from the INO.
 *
 * Functions (See comments below and #defines above:
 *   bool available()
 *   float read(unsigned int binNumber)
 *   float read(unsigned int binFirst, unsigned int binLast)
 *   int windowFunction(int wNum)
 *   int windowFunction(int wNum, float _kdb)  // Kaiser only
 *   float* getData(void)
 *   float* getWindow(void)
 *   void putWindow(float *pwin)
 *   void setNAverage(int NAve)   // >=1
 *   void setOutputType(int _type)
 *   void setXAxis(uint8_t _xAxis)  // 0, 1, 2, 3
 *
 * x-Axis direction and offset per setXAxis(xAxis) for sine to I
 * and cosine to Q.
 *   If xAxis=0  f=fs/2 in middle, f=0 on right edge
 *   If xAxis=1  f=fs/2 in middle, f=0 on left edge
 *   If xAxis=2  f=fs/2 on left edge, f=0 in middle
 *   If xAxis=3  f=fs/2 on right edgr, f=0 in middle
 * If there is 180 degree phase shift to I or Q these all get reversed.
 *
 * Timing, max is longest update() time:
 *   T3.6 Windowed, Power Out, 285 uSec max
 *   T3.6 Windowed, dBFS out, 590 uSec max
 *   T3.6 No Window saves 28 uSec for any output.
 *   T4.0 Windowed, dBFS Out, 120 uSec
 *
 * Scaling:
 *   Full scale for floating point DSP is a nebulous concept.  Normally the
 *   full scale is -1.0 to +1.0.  This is an unscaled FFT and for a sine
 *   wave centered in frequency on a bin and of FS amplitude, the power
 *   at that center bin will grow by 256^2/4 = 16384 without windowing.
 *   Windowing loss cuts this down.  The RMS level can grow to sqrt(16384)
 *   or 128.  The dBFS has been scaled to make this max value 0 dBFS by
 *   removing 42.1 dB.  With floating point, the dynamic range is maintained
 *   no matter how it is scaled, but this factor needs to be considered
 *   when building the INO.
 */

#ifndef analyze_fft256iq_h_
#define analyze_fft256iq_h_

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"
#include "mathDSP_F32.h"
#if defined(__IMXRT1062__)
#include "arm_const_structs.h"
#endif

#define FFT_RMS 0
#define FFT_POWER 1
#define FFT_DBFS 2

#define NO_WINDOW 0
#define AudioWindowNone 0
#define AudioWindowHanning256 1
#define AudioWindowKaiser256 2
#define AudioWindowBlackmanHarris256 3

class AudioAnalyzeFFT256_IQ_F32 : public AudioStream_F32  {
//GUI: inputs:2, outputs:4  //this line used for automatic generation of GUI node
//GUI: shortName:AnalyzeFFT256IQ
public:
    AudioAnalyzeFFT256_IQ_F32() : AudioStream_F32(2, inputQueueArray) {
        // __MK20DX128__ T_LC;  __MKL26Z64__ T3.0;  __MK20DX256__T3.1 and T3.2
        // __MK64FX512__) T3.5; __MK66FX1M0__ T3.6; __IMXRT1062__ T4.0 and T4.1
#if defined(__IMXRT1062__)
        // Teensy4 core library has the right files for new FFT
        // arm CMSIS library has predefined structures of type arm_cfft_instance_f32
        Sfft = arm_cfft_sR_f32_len256;   // This is one of the structures
#else
        arm_cfft_radix4_init_f32(&fft_inst, 256, 0, 1); // for T3.x
#endif
        useHanningWindow();
    }
    // There is no varient for "settings," as blocks other than 128 are
    // not supported and, nothing depends on sample rate so we don't need that.


    bool available() {
        if (outputflag == true) {
            outputflag = false;
            return true;
        }
        return false;
    }

    float read(unsigned int binNumber) {
        if (binNumber>255 || binNumber<0) return 0.0;
        return output[binNumber];
    }

    // Return sum of several bins. Normally use with power output.
    // This produces the equivalent of bigger bins.
    float read(unsigned int binFirst, unsigned int binLast) {
        if (binFirst > binLast) {
            unsigned int tmp = binLast;
            binLast = binFirst;
            binFirst = tmp;
        }
        if (binFirst > 255) return 0.0f;
        if (binLast > 255) binLast = 255;
        float sum = 0.0f;
        do {
            sum += output[binFirst++];
        } while (binFirst <= binLast);
        return sum;
    }

    int windowFunction(int wNum) {
      if(wNum == AudioWindowKaiser256)
         return -1;                 // Kaiser needs the kdb
      windowFunction(wNum, 0.0f);
      return 0;
    }

    int windowFunction(int wNum, float _kdb) {
      float kd;
      pWin = window;
      if(wNum == NO_WINDOW)
         pWin = NULL;
      else if (wNum == AudioWindowKaiser256)  {
         if(_kdb<20.0f)
            kd = 20.0f;
         else
            kd = _kdb;
         useKaiserWindow(kd);
         }
      else if (wNum == AudioWindowBlackmanHarris256)
         useBHWindow();
     else
         useHanningWindow();   // Default
     return 0;
     }

    // Fast pointer transfer.  Be aware that the data will go away
    // after the next 256 data points occur.
    float* getData(void)  {
       return output;
       }

    // You can use this to design windows
    float* getWindow(void)  {
       return window;
       }

    // Bring custom window from the INO
    void putWindow(float *pwin)  {
       float *p = window;
       for(int i=0; i<256; i++)
          *p++ = *pwin++;   // Copy for the FFT
       }

    // Output RMS (default) Power or dBFS
    void setOutputType(int _type)  {
       outputType = _type;
       }

    // Output power (non-coherent) averaging
    // i.e., the number of FFT powers averaged in the output
    void setNAverage(int _nAverage)  {
       nAverage = _nAverage;
       }

    // xAxis, bit 0 left/right;  bit 1 low to high;  default 0X03
    void setXAxis(uint8_t _xAxis)  {
       xAxis = _xAxis;
       }

    virtual void update(void);

private:
  float output[256];
  float window[256];
  float *pWin = window;
  float fft_buffer[512];
  float sumsq[256];  // Avoid re-use of output[]
  uint8_t state = 0;
  bool outputflag = false;
  audio_block_f32_t *inputQueueArray[2];
  audio_block_f32_t *prevblock_i,*prevblock_q;
#if defined(__IMXRT1062__)
  // For T4.x
  // const static arm_cfft_instance_f32   arm_cfft_sR_f32_len256;
  arm_cfft_instance_f32 Sfft;
#else
  arm_cfft_radix4_instance_f32 fft_inst;
#endif
  int outputType = FFT_RMS;  //Same type as I16 version init
  int count = 0;
  int nAverage = 1;
  uint8_t xAxis = 3;

    // The Hann window is a good all-around window
    void useHanningWindow(void) {
        for (int i=0; i < 256; i++) {
           // 2*PI/255 = 0.0246399424
           window[i] = 0.5*(1.0 - cosf(0.0246399424*(float)i));
        }
    }

    // Blackman-Harris produces a first sidelobe more than 90 dB down.
    // The price is a bandwidth of about 2 bins.  Very useful at times.
    void useBHWindow(void) {
        for (int i=0; i < 256; i++) {
           float kx = 0.0246399424;  // 2*PI/255
           int ix = (float) i;
           window[i] = 0.35875 -
                       0.48829*cosf(     kx*ix) +
                       0.14128*cosf(2.0f*kx*ix) -
                       0.01168*cosf(3.0f*kx*ix);
        }
    }

    /* The windowing function here is that of James Kaiser.  This has a number
     * of desirable features. The sidelobes drop off as the frequency away from a transition.
     * Also, the tradeoff of sidelobe level versus cutoff rate is variable.
     * Here we specify it in terms of kdb, the highest sidelobe, in dB, next to a sharp cutoff. For
     * calculating the windowing vector, we need a parameter beta, found as follows:
     */
    void useKaiserWindow(float kdb)  {
       float32_t beta, kbes, xn2;
       mathDSP_F32 mathEqualizer;  // For Bessel function

       if (kdb < 20.0f)
           beta = 0.0;
       else
           beta = -2.17+0.17153*kdb-0.0002841*kdb*kdb; // Within a dB or so

       // Note: i0f is the fp zero'th order modified Bessel function (see mathDSP_F32.h)
       kbes = 1.0f / mathEqualizer.i0f(beta);      // An additional derived parameter used in loop
       for (int n=0; n<128; n++) {
          xn2 = 0.5f+(float32_t)n;
          // 4/(255^2)=0.000061514802f
          xn2 = 0.000061514802f*xn2*xn2;
          window[127 - n]=kbes*(mathEqualizer.i0f(beta*sqrtf(1.0-xn2)));
          window[128 + n] = window[255 - n];
       }
    }
  };
#endif
