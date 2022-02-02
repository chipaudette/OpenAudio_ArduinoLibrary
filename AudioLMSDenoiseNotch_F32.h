/*
 * AudioLMSDenoiseNotch_F32.h
 *
 * Created: Bob Larkin, January 2022
 * Purpose; LMS DeNoise and Auto-notch for audio.
 * Assumes floating-point data.
 *
 * 22 January 2022   copyright (c)Robert Larkin 2022
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development notice, and this permission
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

 /* *** Notes  ***
  * The LMS DeNoise is effective for improving the signal-to-noise ratio (S/N)
  * when the input S/N is reasonably high.  When the signal is "buried" in the noise
  * it is much less effective.  Thus it is effective as a radio "squelch" for SSB.
  *
  * The auto-notch is very effective at removing annoying tones when they are
  * reasonably strong.  Again for radio systems, this can be quite useful.
  * The initialization selects whether DeNoise or AutoNotch is used.  It makes
  * no sense to use both at once as, in a perfect world, that would remove everything.
  *
  * The LMS algorithm for optimization was first proposed by
  * Widrow and Hoff in 1960.
  * It has been applied extensively due to its simplicity.  The form here
  * optimizes the coefficients of a FIR filter to recognize any coherency
  * to the input signal.  This can be use to reduce non-coherent noise by
  * using the FIR filter output.  Alternatively, the input signal can be
  * subtracted from the FIR filter output to remove coherent signals,
  * producing the so called "auto-notch."
  *
  * This particular write of the denoise and auto-notch traces back to
  * Johan Forrer, KC7WW, per September 1994 QEX.  From there it was used
  * in the DSP-10 project, http://www.janbob.com/electron/dsp10/dsp10.htm
  *
  * The normalized version of coefficient update is generally best.  If it
  * is not desired, it can be removed at compile time by commenting out
  * "#define LMS_NORMALIZE" below.
  *
  * Initialization also sets the size of the FIR  buffer used to filter signal
  * and noise.  Small buffers respond to change quickly.  Large buffers can work
  * on lower audio frequencies.  Experiment with this. The FIR buffer is set in
  * powers of 2, such as 32, 64 or 128.  The maximum value is set at compile
  * time by the #define MAX_FIR (default 128).
  *
  * Initialization sets the decorrelation delay size.  If the LMS is preceded by
  * a narrow band filter, this delay must be greater.  Wide band systems can
  * work with less delay.  Experiment with this, also. The DELAY buffer size
  * can be any value from 2 to MAX DELAY.  The maximum value is set at compile
  * time by the #define MAX_DELAY (default 16).
  *
  * This block behaves as a pass-through filter with one input and one output.
  *
  * There are two parameters that are set in the .ino via the function
  * setParameters(float32_t beta, float32_t decay)
  * The first, beta determines the rate of convergence of the coefficients.
  * This changes the "sound" of the audio and normally is one of a radio's
  * front panel adjustments.  The second parameter, decay, slowly turns
  * the algorithm off when signals are absent.  It is normally very
  * slightly less than 1.0.  This can also change the "sound."
  *
  * The Teensy 3.6 needs 690 microseconds per 128 block update using a FIR
  * buffer size of 32.  It needs 1335 microseconds using 64 FIR Buffer.
  * Note that the ARM library LMS routines might improve these
  * numbers. Those  routines use double buffer sizes to remove the
  * need for the circular buffering used here.  It also uses x4 loop un-wrapping.
  * The price is a signifigantly more complex setup involving moving of data
  * and the added memory.
  *
  * Teensy 4.x needs 140 microseconds for 32 FIR word buffer size,
  * 270 for 64, and 529 microseconds for 128.
  *
  * All timing was done with a delay buffer of 4, but this size has
  * very little effect, anyway. Normalization was off, also, but
  * again, this has a minor effect.
  */

#ifndef _AudioLMSDenoiseNotch_F32_h
#define _AudioLMSDenoiseNotch_F32_h

#include <AudioStream_F32.h>
#include "arm_math.h"

// Default is to use the normalized form of coefficient update
#define LMS_NORMALIZE

#define MAX_FIR  256
#define MAX_DELAY 16
#define DENOISE  1
#define NOTCH    2

class AudioLMSDenoiseNotch_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  public:
    //constructor
    AudioLMSDenoiseNotch_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {};
    AudioLMSDenoiseNotch_F32(const AudioSettings_F32 &settings) :
                                     AudioStream_F32(1, inputQueueArray_f32) {};

    uint16_t initializeLMS(uint16_t _what, uint16_t _lengthDataF, uint16_t _lengthDataD)
        {
        what = _what;
        if(what != DENOISE && what != NOTCH)  what = DENOISE;
        lengthDataF = powf(2.0f, log2f(_lengthDataF)+0.000001f);  //Make sure a power of 2
        lengthDataF = (lengthDataF>MAX_FIR ? MAX_FIR : lengthDataF);  // Limit length
        kMask = lengthDataF - 1;
        lengthDataD = _lengthDataD;
        lengthDataD = (lengthDataD>MAX_DELAY ? MAX_DELAY : lengthDataD);  // Limit length
#ifdef LMS_NORMALIZE
        for(int i=0; i<128; i++) powerNorm[i] = 0.01f;
        pNorm = 0.01f * 128.0f;
#endif
        return lengthDataF;
        }

    // If setEnable is false the LMS object update() becomes pass-though.
    void enable(bool setEnable) {
        if(setEnable)  doLMS=true;
        else  doLMS=false;
        }

    void setParameters(float32_t _beta, float32_t _decay)
        {
        beta = _beta;
        if(beta>=1.0f) beta = 0.999999f;
        if(beta<0.000001) beta = 0.000001f;
        decay = _decay;
        if(decay>=1.0f) decay = 0.999999f;
        if(decay<0.000001) decay = 0.000001f;
        }

    virtual void update(void);

  private:
    audio_block_f32_t *inputQueueArray_f32[1]; //memory pointer for the input to this module
    uint16_t what = DENOISE;     // DENOISE or NOTCH
    bool doLMS = false;

    float32_t dataD[16];           // Can be made less than 16 by lengthDataD
    uint16_t kNextD = 0;
    uint16_t kOffsetD = 0;
    uint16_t lengthDataD = 4;      // Any value, 2 to MAX_DELAY

    float32_t coeff[128];
#ifdef LMS_NORMALIZE
    float32_t powerNorm[128];
    float32_t pNorm = 0.0f;
#endif

    // dataF[] is arranged, by added variables kOffset and
    // lengthDataF, to be circular. A power-of-2 mask makes it circular.
    float32_t dataF[128];           // Can be made less than 128 by lengthDataF
    float32_t dataOutF = 0.0f;
    uint16_t kOffsetF = 0;
    uint16_t lengthDataF = 64;
    uint16_t kMask = 63;

    float32_t beta = 0.03f;
    float32_t decay = 0.995f;
    uint16_t numLeak = 0;
};
#endif
