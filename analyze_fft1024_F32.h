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
  * Only Hann window for now.
  */

#ifndef analyze_fft1024_F32_h_
#define analyze_fft1024_F32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"
#define FFT_RMS 0
#define FFT_POWER 1
#define FFT_DBFS 2

/*  // windows.c
extern "C" {
extern const int16_t AudioWindowHanning1024[];
extern const int16_t AudioWindowBartlett1024[];
extern const int16_t AudioWindowBlackman1024[];
extern const int16_t AudioWindowFlattop1024[];
extern const int16_t AudioWindowBlackmanHarris1024[];
extern const int16_t AudioWindowNuttall1024[];
extern const int16_t AudioWindowBlackmanNuttall1024[];
extern const int16_t AudioWindowWelch1024[];
extern const int16_t AudioWindowHamming1024[];
extern const int16_t AudioWindowCosine1024[];
extern const int16_t AudioWindowTukey1024[];  )
*/

class AudioAnalyzeFFT1024_F32 : public AudioStream_F32  {
//GUI: inputs:1, outputs:0  //this line used for automatic generation of GUI node
//GUI: shortName:AnalyzeFFT1024
public:
    AudioAnalyzeFFT1024_F32() : AudioStream_F32(1, inputQueueArray) {
        arm_cfft_radix4_init_f32(&fft_inst, 1024, 0, 1);
        useHanningWindow();  // Revisit this for more flexibility  <<<<<
    }
    bool available() {
        if (outputflag == true) {
            outputflag = false;
            return true;
        }
        return false;
    }
    float read(unsigned int binNumber) {
        if (binNumber>511 || binNumber<0) return 0.0;
        return output[binNumber];
    }
    float read(unsigned int binFirst, unsigned int binLast) {
        if (binFirst > binLast) {
            unsigned int tmp = binLast;
            binLast = binFirst;
            binFirst = tmp;
        }
        if (binFirst > 511) return 0.0;
        if (binLast > 511) binLast = 511;
        uint32_t sum = 0;
        do {
            sum += output[binFirst++];
        } while (binFirst <= binLast);
        return (float)sum * (1.0 / 16384.0);
    }

    void useHanningWindow(void) {
        for (int i=0; i < 1024; i++) {
           // 2*PI/1023 = 0.006141921
           window[i] = 0.5*(1.0 - cosf(0.006141921f*(float)i));
        }
    }

//    void windowFunction(const float *w) {
//        window = w;
//    }

    void setOutputType(int _type)  {
       outputType = _type;
       }

    virtual void update(void);

    float output[512];
private:
    // void init(void);
    float window[1024];    int doPrint = 0;
    audio_block_f32_t *blocklist[8];
    float fft_buffer[2048];
    uint8_t state = 0;
    bool outputflag = false;
    audio_block_f32_t *inputQueueArray[1];
    arm_cfft_radix4_instance_f32 fft_inst;
    int outputType = FFT_RMS;  //Same type as I16 version has
};

#endif
