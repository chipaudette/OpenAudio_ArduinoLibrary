/* analyze_fft1024_F32.h      Converted from Teensy I16 Audio Library
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

/* Moved directly I16 to F32. Bob Larkin 16 Feb 2021
 * Does real input FFT of 1024 points. Output is not audio, and is magnitude
 * only.  Multiple output formats of RMS (same as I16 version, and default),
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
 *   void setOutputType(int _type)
 *   void setNAverage(int nAverage)
 *
 * Timing, max is longest update() time:
 *   T3.6 Windowed, Power Out, 975 uSec
 *   T3.6 Windowed, RMS out,  1016 uSec
 *   T3.6 Windowed, dBFS out, 1591 uSec
 *   No Window saves 60 uSec on T3.6 for any output.
 *   T4.0 Windowed, Ave=1, Power Out, 156 uSec
 *   T4.0 Windowed, Ave=1, dBFS Out,  302 uSec
 * Scaling:
 *   Full scale for floating point DSP is a nebulous concept.  Normally the
 *   full scale is -1.0 to +1.0.  This is an unscaled FFT and for a sine
 *   wave centered in frequency on a bin and of FS amplitude, the power
 *   at that center bin will grow by 1024^2/4 = 262144 without windowing.
 *   Windowing loss cuts this down.  The RMS level can grow to sqrt(262144)
 *   or 512.  The dBFS has been scaled to make this max value 0 dBFS by
 *   removing 54.2 dB.  With floating point, the dynamic range is maintained
 *   no matter how it is scaled, but this factor needs to be considered
 *   when building the INO.
 */
// Fixed float/int problem in read(first, last).  RSL 3 Mar 21

#ifndef analyze_fft1024_F32_h_
#define analyze_fft1024_F32_h_

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
#define AudioWindowHanning1024 1
#define AudioWindowKaiser1024 2
#define AudioWindowBlackmanHarris1024 3

class AudioAnalyzeFFT1024_F32 : public AudioStream_F32  {
//GUI: inputs:1, outputs:0  //this line used for automatic generation of GUI node
//GUI: shortName:FFT1024
public:
    AudioAnalyzeFFT1024_F32() : AudioStream_F32(1, inputQueueArray) {
        // __MK20DX128__ T_LC;  __MKL26Z64__ T3.0;  __MK20DX256__T3.1 and T3.2
        // __MK64FX512__) T3.5; __MK66FX1M0__ T3.6; __IMXRT1062__ T4.0 and T4.1
#if defined(__IMXRT1062__)
        // Teensy4 core library has the right files for new FFT
        // arm CMSIS library has predefined structures of type arm_cfft_instance_f32
        Sfft = arm_cfft_sR_f32_len1024;   // This is one of the structures
#else
        arm_cfft_radix4_init_f32(&fft_inst, 1024, 0, 1); // for T3.x
#endif
        // This is always 128 block size.  Any sample rate.  No use of "setings"
        // Bob:  Revisit this to use CMSIS fast fft in place of cfft.  faster?
        useHanningWindow();
    }

    // Inform that the output is available for read()
    bool available() {
        if (outputflag == true) {
            outputflag = false;
            return true;
            }
        return false;
        }

    // Output data from a single bin is transferred
    float read(unsigned int binNumber) {
        if (binNumber>511 || binNumber<0) return 0.0;
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
        if (binFirst > 511) return 0.0;
        if (binLast > 511) binLast = 511;
        float sum = 0.0f;
        do {
            sum += output[binFirst++];
        } while (binFirst <= binLast);
        return sum;
    }

    int windowFunction(int wNum) {
      if(wNum == AudioWindowKaiser1024)
         return -1;                 // Kaiser needs the kdb
      windowFunction(wNum, 0.0f);
      return 0;
    }

    int windowFunction(int wNum, float _kdb) {
      float kd;
      pWin = window;
      if(wNum == NO_WINDOW)
         pWin = NULL;
      else if (wNum == AudioWindowKaiser1024)  {
         if(_kdb<20.0f)
            kd = 20.0f;
         else
            kd = _kdb;
         useKaiserWindow(kd);
         }
      else if (wNum == AudioWindowBlackmanHarris1024)
         useBHWindow();
     else
         useHanningWindow();   // Default
     return 0;
     }

    // Fast pointer transfer.  Be aware that the data will go away
    // after the next 512 data points occur.
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
       for(int i=0; i<1024; i++)
          *p++ = *pwin++;
       }

    // Output RMS (default) Power or dBFS
    void setOutputType(int _type)  {
       outputType = _type;
       }

    // Output power (non-coherent) averaging
    void setNAverage(int _nAverage)  {
       nAverage = _nAverage;
       }

    virtual void update(void);

private:
    float output[512];
    float sumsq[512];
    float window[1024];
    float *pWin = window;
    audio_block_f32_t *blocklist[8];
    float fft_buffer[2048];
    uint8_t state = 0;
    bool outputflag = false;
    audio_block_f32_t *inputQueueArray[1];
#if defined(__IMXRT1062__)
  // For T4.x
  // const static arm_cfft_instance_f32   arm_cfft_sR_f32_len1024;
  arm_cfft_instance_f32 Sfft;
#else
  arm_cfft_radix4_instance_f32 fft_inst;
#endif
    int outputType = FFT_RMS;  //Same type as I16 version init
    int nAverage = 1;
    int count = 0;     // used to average for nAverage of powers

    // The Hann window is a good all-around window
    void useHanningWindow(void) {
        for (int i=0; i < 1024; i++) {
           // 2*PI/1023 = 0.006141921
           window[i] = 0.5*(1.0 - cosf(0.006141921f*(float)i));
        }
    }

    // Blackman-Harris produces a first sidelobe more than 90 dB down.
    // The price is a bandwidth of about 2 bins.  Very useful at times.
    void useBHWindow(void) {
        for (int i=0; i < 1024; i++) {
           float kx = 0.006141921;  // 2*PI/1023
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
       for (int n=0; n<512; n++) {
          xn2 = 0.5f+(float32_t)n;
          // 4/(1023^2)=0.00000382215877f
          xn2 = 0.00000382215877f*xn2*xn2;
          window[511 - n]=kbes*(mathEqualizer.i0f(beta*sqrtf(1.0-xn2)));
          window[512 + n] = window[511 - n];
          }
       }
    };
#endif
