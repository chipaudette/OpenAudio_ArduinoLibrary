/*
 *   analyze_fft4096_iqem_F32.h    Assembled by Bob Larkin   18 Feb 2022
 *
 * External Memory - INO supplied memory arrays.  Windows are half width.
 *
 *  Note: Teensy 4.x ONLY, 3.x not supported
 *
 * Does Fast Fourier Transform of a 4096 point complex (I-Q) input.
 * Output is one of three measures of the power in each of the 4096
 * output bins, Power, RMS level or dB relative to a full scale
 * sine wave.  Windowing of the input data is provided for to reduce
 * spreading of the power in the output bins.  All inputs are Teensy
 * floating point extension (_F32) and all outputs are floating point.
 *
 * Features include:
 *   * I and Q inputs are OpenAudio_Arduino Library F32 compatible.
 *   * FFT output for every 2048 inputs to overlapped FFTs to
 *     compensate for windowing.
 *   * Windowing None, Hann, Kaiser and Blackman-Harris.
 *   * Multiple bin-sum output to simulate wider bins.
 *   * Power averaging of multiple FFT
 *
 * Conversion Copyright (c) 2022 Bob Larkin
 * Same MIT license as PJRC:
 *
 * From original real FFT:
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

/* Does complex input FFT of 4096 points.  Multiple non-audio (via functions)
 * output formats of RMS (same as I16 version, and default),
 * Power or dBFS (full scale).  Output can be bin by bin or a pointer to
 * the output array is available.  Several window functions are provided by
 * in-class design, or a custom window can be provided from the INO.
 *
 * Memory for IQem FFT.  The large blocks of memory must be declared in the INO.
 * This typically looks like:
 * float32_t  fftOutput[4096];  // Array used for FFT Output to the INO program
 * float32_t  window[2048];     // Windows reduce sidelobes with FFT's *Half Size*
 * float32_t  fftBuffer[8192];  // Used by FFT, 4096 real, 4096 imag, interleaved
 * float32_t  sumsq[4096];      // Required ONLY if power averaging is being done
 *
 * These blocks of memory are communicated to the FFT in the object creation, that
 * might look like:
 *   AudioAnalyzeFFT4096_IQEM_F32 myFFT(fftOutput, window, fftBuffer);
 * or, if power averaging is used, the extra parameter is needed as:
 *   AudioAnalyzeFFT4096_IQEM_F32 myFFT(fftOutput, window, fftBuffer, sumsq);
 *
 * The memory arrays must be declared before the FFT object.  About 74 kBytes are
 * required if power averaging is used and about 58 kBytes without power averaging.
 *
 * In addition, this requires 64 AudioMemory_F32 which work out to about  an
 * additional 33 kBytes of memory.
 *
 * If several FFT sizes are used, one at a time, the memory can be shared.  Probably
 * the simplest way to do this with a Teensy is to set up C-language unions.
 *
 * Functions (See comments below and #defines above:
 *   bool available()
 *   float read(unsigned int binNumber)
 *   float read(unsigned int binFirst, unsigned int binLast)
 *   int windowFunction(int wNum)
 *   int windowFunction(int wNum, float _kdb)  // Kaiser only
 *   void setNAverage(int NAve)   // >=1
 *   void setOutputType(int _type)
 *   void setXAxis(uint8_t _xAxis)  // 0, 1, 2, 3
 *
 * x-Axis direction and offset per setXAxis(xAxis) for sine to I
 * and cosine to Q:
 *
 *   If xAxis=0  f=fs/2 in middle, f=0 on right edge
 *   If xAxis=1  f=fs/2 in middle, f=0 on left edge
 *   If xAxis=2  f=fs/2 on left edge, f=0 in middle
 *   If xAxis=3  f=fs/2 on right edgr, f=0 in middle
 *
 * Timing, maximum microseconds per update() over the 16 updates,
 * and the average percent processor use for 44.1 kHz sample rate and Nave=1:
 *   T4.0 Windowed, dBFS Out (FFT_DBFS), 710 uSec, Ave 4.64%
 *   T4.0 Windowed, Power Out (FFT_POWER), 530 uSec, Ave 1.7%
 *   T4.0 Windowed, RMS Out, (FFT_RMS) 530 uSec, Ave 1.92%
 * Nave greater than 1 decreases the average processor load.
 *
 * Windows:  The FFT window array memory is provided by the INO.  Three common and
 * useful window functions, plus no window, can be filled into the array by calling
 * one of the following:
 *   windowFunction(AudioWindowNone);
 *   windowFunction(AudioWindowHanning4096);
 *   windowFunction(AudioWindowKaiser4096);
 *   windowFunction(AudioWindowBlackmanHarris4096);
 * See:  https://en.wikipedia.org/wiki/Window_function
 *
 * To use an alternate window function, just fill it into the array, window, above.
 * It is only half of the window (2048 floats).  It looks like a full window
 * function with the right half missing.  It should start with small
 * values on the left (near[0]) and go to 1.0 at the right ([2048]).
 *
 * As with all library FFT's this one provides overlapping time series.  This
 * tends to compensate for the attenuation at the window edges when doing a sequence
 * of FFT's.  For that reason there can be a new FFT result every 2048 time
 * series data points.
 *
 * Scaling:
 *   Full scale for floating point DSP is a nebulous concept.  Normally the
 *   full scale is -1.0 to +1.0.  This is an unscaled FFT and for a sine
 *   wave centered in frequency on a bin and of FS amplitude, the power
 *   at that center bin will grow by 4096^2/4 = about 4 million without windowing.
 *   Windowing loss cuts this down.  The RMS level can growwithout windowing to
 *   4096.  The dBFS has been scaled to make this max value 0 dBFS by
 *   removing 66.2 dB.  With floating point, the dynamic range is maintained
 *   no matter how it is scaled, but this factor needs to be considered
 *   when building the INO.
 *
 *   22 Feb 2022 Fixed xAxis error, twice!
 */
 /*  Info:
  * __MK20DX128__ T_LC;  __MKL26Z64__ T3.0;  __MK20DX256__T3.1 and T3.2
  * __MK64FX512__) T3.5; __MK66FX1M0__ T3.6; __IMXRT1062__ T4.0 and T4.1 */

#ifndef analyze_fft4096_iqem_h_
#define analyze_fft4096_iqem_h_

// ***************  TEENSY 4.X ONLY   ****************
#if defined(__IMXRT1062__)

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"
#include "mathDSP_F32.h"
#include "arm_const_structs.h"

#define FFT_RMS 0
#define FFT_POWER 1
#define FFT_DBFS 2

#define NO_WINDOW 0
#define AudioWindowNone 0
#define AudioWindowHanning4096 1
#define AudioWindowKaiser4096 2
#define AudioWindowBlackmanHarris4096 3

class AudioAnalyzeFFT4096_IQEM_F32 : public AudioStream_F32  {
//GUI: inputs:2, outputs:0  //this line used for automatic generation of GUI node
//GUI: shortName:FFT4096IQem

public:
    AudioAnalyzeFFT4096_IQEM_F32   // Without sumsq in call for averaging
      (float32_t* _pOutput, float32_t* _pWindow, float32_t* _pFFT_buffer) :
      AudioStream_F32(2, inputQueueArray) {
        pOutput = _pOutput;
        pWindow = _pWindow;
        pFFT_buffer = _pFFT_buffer;
        pSumsq = NULL;
        // Teensy4 core library has the right files for new FFT
        // arm CMSIS library has predefined structures of type arm_cfft_instance_f32
        Sfft = arm_cfft_sR_f32_len4096;   // This is one of the structures
        useHanningWindow();
    }

    AudioAnalyzeFFT4096_IQEM_F32  // Constructor to include sumsq power averaging.
      (float32_t* _pOutput, float32_t* _pWindow, float32_t* _pFFT_buffer,
       float32_t* _pSumsq) :
       AudioStream_F32(2, inputQueueArray) {
        pOutput = _pOutput;
        pWindow = _pWindow;
        pFFT_buffer = _pFFT_buffer;
        pSumsq = _pSumsq;
        // Teensy4 core library has the right files for new FFT
        // arm CMSIS library has predefined structures of type arm_cfft_instance_f32
        Sfft = arm_cfft_sR_f32_len4096;   // This is one of the structures
        useHanningWindow();
    }

    // There is no varient for "settings," as blocks other than 128 are
    // not supported and, nothing depends on sample rate so we don't need that.

    // Returns true when output data is available.
    bool available() {
#if defined(__IMXRT1062__)
        if (outputflag == true) {
            outputflag = false;  // No double returns
            return true;
        }
        return false;
#else
        // Don't know how you got this far, but....
        Serial.println("Teensy 3.x NOT SUPPORTED");
        return false;
#endif
    }

    // Returns a single bin output
    float read(unsigned int binNumber) {
        if (binNumber>4095 || binNumber<0) return 0.0;
        return *(pOutput + binNumber);
    }

    // Return sum of several bins. Normally use with power output.
    // This produces the equivalent of bigger bins.
    float read(unsigned int binFirst, unsigned int binLast) {
        if (binFirst > binLast) {
            unsigned int tmp = binLast;
            binLast = binFirst;
            binFirst = tmp;
        }
        if (binFirst > 4095) return 0.0;
        if (binLast > 4095) binLast = 4095;
        float sum = 0;
        do {
            sum += *(pOutput + binFirst++);
        } while (binFirst <= binLast);
        return sum;
    }

    // Sets None, Hann, or Blackman-Harris window with no parameter
    int windowFunction(int _wNum) {
	   wNum = _wNum;
       if(wNum == AudioWindowKaiser4096)
          return -1;                 // Kaiser needs the kdb
       windowFunction(wNum, 0.0f);
       return 0;
    }

    int windowFunction(int _wNum, float _kdb) { // Kaiser case
      float kd;
      wNum = _wNum;
      if (wNum == AudioWindowKaiser4096)  {
         if(_kdb<20.0f)
            kd = 20.0f;
         else
            kd = _kdb;
         useKaiserWindow(kd);
         }
      else if (wNum == AudioWindowBlackmanHarris4096)
         useBHWindow();
      else
         useHanningWindow();   // Default
     return 0;
     }

    // Number of FFT averaged in the output
    void setNAverage(int _nAverage)  {
       if(!(pSumsq==NULL))  // We can average because we have memory.
           nAverage = _nAverage;
       }

    // Output RMS (default), power or dBFS (FFT_RMS, FFT_POWER, FFT_DBFS)
    void setOutputType(int _type)  {
       outputType = _type;
       }

    // xAxis, bit 0 left/right;  bit 1 low to high;  default 0X03
    void setXAxis(uint8_t _xAxis)  {
	   xAxis = _xAxis;
       }

  virtual void update(void);

private:
  float32_t  *pOutput, *pWindow, *pFFT_buffer;
  float32_t  *pSumsq;
  int wNum = AudioWindowHanning4096;
  uint8_t state = 0;
  bool outputflag = false;
  audio_block_f32_t *inputQueueArray[2];
  audio_block_f32_t *blocklist_i[32];
  audio_block_f32_t *blocklist_q[32];
  // For T4.x
  // const static arm_cfft_instance_f32   arm_cfft_sR_f32_len1024;
  arm_cfft_instance_f32 Sfft;
  int outputType = FFT_RMS;  //Same type as I16 version init
  int count = 0;
  int nAverage = 1;
  uint8_t xAxis = 0x03;   // See discussion above

    // The Hann window is a good all-around window
    // This can be used with zero-bias frequency interpolation.
    // pWidow points to INO supplied buffer. 4096 for now.  MAKE 2048 <<<<<<<<<<<<<<<<
    void useHanningWindow(void) {
		if(!pWindow) return;   // No placefor a window
        for (int i=0; i < 2048; i++) {
           // 2*PI/4095 = 0.00153435538
           *(pWindow + i) = 0.5*(1.0 - cosf(0.00153435538f*(float)i));
        }
    }

    // Blackman-Harris produces a first sidelobe more than 90 dB down.
    // The price is a bandwidth of about 2 bins.  Very useful at times.
    void useBHWindow(void) {
		if(!pWindow) return;
        for (int i=0; i < 2048; i++) {
           float kx = 0.00153435538f;  // 2*PI/4095
           int ix = (float) i;
           *(pWindow + i) = 0.35875 -
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

	   if(!pWindow) return;

       if (kdb < 20.0f)
           beta = 0.0;
       else
           beta = -2.17+0.17153*kdb-0.0002841*kdb*kdb; // Within a dB or so

       // Note: i0f is the fp zero'th order modified Bessel function (see mathDSP_F32.h)
       kbes = 1.0f / mathEqualizer.i0f(beta); // An additional derived parameter used in loop
       for (int n=0; n<2048; n++) {
          xn2 = 0.5f+(float32_t)n;
          // 4/(4095^2) = 2.3853504E-7
          xn2 = 2.3853504E-7*xn2*xn2;
          *(pWindow + 2047 - n) = kbes*(mathEqualizer.i0f(beta*sqrtf(1.0-xn2)));
       }
    }
  };
#endif
#endif
