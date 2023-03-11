/*
 *   radioFT8Demodulator_F32.h    Assembled by Bob Larkin   11 Sept 2022
 *
 *  Note: Teensy 4.x Only, 3.x not supported
 *
 * (c) 2021 Bob Larkin
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

/* *** CREDITS ***
 * There have been many contributors to this FT8 project.  I will not
 * catch them all, but a try is neeeded.  I picked from
 * multiple sources in making the transmission process consistent with the
 * 128-block Teensy Audio Library.  But, everything I did was a translation
 * of existing software.  The "specification" for FT8 is the paper, "The FT4
 * and FT8 Communications Protocols," by Steve Franke, K9AN, Bill Somerville,
 * G4WJS and Joe Taylor, K1JT, QEX July/August 2020, pp7-17.  I was guided
 * in the right direction for this implementation by Farhan, VU2ESE
 * and Charley, W5BAA. This led to a pair of excellent Raspberry Pi sources,
 * https://github.com/kgoba/ft8_lib and
 * https://github.com/WB2CBA/W5BAA-FT8-POCKET-TERMINAL
 * An FT8 derivative of the first web site, using the Raspberry Pi,
 * is Farhan's sBitx:
 * https://github.com/afarhan/sbitx
 * For those not familiar with FT8, it is a widely used amateur radio
 * communications format.  A search of FT8 will lead to much information.
 *
 * Notes:
 * 1. Sampling rate: The entire FT8 Demodulator runs at a decimated
 * 6400 samples/sec. At this time, only 48 and 96 kHz sample rates are
 * supported.  For 48 kHz there is
 * an interpolate to 96 kHz followed by decimation to 19.2 and 6.4 kHz.
 * For 96 kHz there is no interpolation.
 *
 * 2. In order to process the decoding of FT8 as rapidly as possibly,
 * after the 0.16 seconds of data are acquired (2048 data points),
 * this library class only does the process that must be done in real
 * time, either because of data un-availability or because the full data
 * set for 0.16 sec is needed.  Thus this class does the two stages
 * of decimation in time needed to reduce the data rate and does the
 * saving of signal samples to an array.
 *
 * 3. To allow two FFT's, offset in time by 0.16sec / 2, we need to gather
 * 2048 data points every 0.08 seconds with 50% overlap  in data.
 * These are put into an array that is supplied by the calling
 * INO/CPP program.  It is arrange as signalData[2][1024]
 * for convenience.  The indices of the first  of two is returned
 * when data is available.  In order to not need dual large storage, an
 * 18 float auxiliary storage is provided to store data acquired during
 * the "first two" update period (5.3 or 2.7 mSec for 48 or
 * 96 kHz sample rate).  This gives time for an FFT
 * before altering the signal data.
 *
 * 4. This class does not provide true (absolute) clock timing.
 * The startReceive() function should be called when it is time
 * for a new reception period.  This class will start that at the
 * next 128 audio sample period.
 *
 * 5. Update time required for a T4.x is    uSec.
 */

// The Charley Hill W5BAA_INTERFACE is from the Pocket-FT9 project and
// transfers data as 128 int's.  It is included as a compile time
// option to allow testing with the Pocket-FT8 Teensy software.
// The default interface transfers the collected data as a pointer to
// 2048 F32 float's ready for windowing and FFT's.  Both
// interfaces includes 50% overlap of the data to correct for window losses.

// Rev 16 Jan 2023 Corrected position of endif for T4.x only  Bob

// #define W5BAA_INTERFACE

#ifndef radioFT8Demodulator_h_
#define radioFT8Demodulator_h_

#define SR_NONE   0
#define SR48000  1
#define SR96000  2

// ***************  TEENSY 4.X ONLY   ****************
#if defined(__IMXRT1062__)

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"

// NOTE Changed class name to start with capital "R"  RSL 7 Nov 2022

class RadioFT8Demodulator_F32 : public AudioStream_F32  {
//GUI: inputs:2, outputs:4  //this line used for automatic generation of GUI node
//GUI: shortName:FFT2048IQ
public:
   RadioFT8Demodulator_F32() : AudioStream_F32(1, inputQueueArray_f32) {


      }
   // There is no varient for "settings," as blocks other than 128 are
   // not supported               FIX.   <<<<<<<<<<<<<<<<<<<<<

   void initialize(void)  {
      // Initialize 2 FIR instances (ARM DSP Math Library)
      //arm_fir_init_f32 (arm_fir_instance_f32 *S, uint16_t numTaps,
      //          const float32_t *pCoeffs, float32_t *pState, uint32_t blockSize)
      arm_fir_init_f32(&fir_inst1, 55,  &firDecimate1[0], &dec1FIRWork[0], 128UL);
      arm_fir_init_f32(&fir_inst2, 167, &firDecimate2[0], &dec2FIRWork[0], 128UL);
   // CHECK SAMPLE RATE AND SET index  <<<<<<<<<<<<<

   FT8DemodInit = true;
#ifdef W5BAA_INTERFACE
   gettingData = true;
#endif

      }

   // Following reflects that there are only 2 supported sample rates.
   // IMPORTANT: This changes constants for FT8 Receive, only.  It does not
   // change system-wide audio sample rate.  Use AudioSettings_F32.
   void setSampleRate_Hz(const float32_t &fs_Hz) {
      sampleRateHz = fs_Hz;
      if(sampleRateHz>47900.0f && sampleRateHz<48100.0f)
         {
         sampleRateHz = 48000.0f;
         srIndex = SR48000;
         }
      else if(sampleRateHz>95900.0f && sampleRateHz<96100.0f)
         {
         sampleRateHz = 96000.0f;
         srIndex = SR96000;
         }
      else
         {
         Serial.println("Unsupported sample rate, FT8 will not receive.");\
         srIndex = SR_NONE;
         }
      }

   void decimate15(void);

   bool powerAvailable(void)  {
      if(powAvail) return true;
      return false;
      }

   // Read the dB power level *once* per 128 samples at 6.4kHz
   float32_t powerRead(void)  {
      if(powAvail)
         {
         powAvail = false;
         return powerOut;
         }
      else
         return -200.0f;
      }

#ifdef W5BAA_INTERFACE
   int queueAvailable(void)  {  // W5BAA_INTERFACE only
      uint32_t h, t;
      h = head;
      t = tail;   //Serial.print("@queueAvailable h, t= ");  Serial.print(h); Serial.print(", "); Serial.println(t);
      if (h >= t)
         return h - t;
      return max_buffers + h - t;
      }

   void queueClear(void)  {  // W5BAA_INTERFACE only
      uint32_t t;
      if (userblock)
         {
         AudioStream::release(userblock);
         userblock = NULL;
         }
      t = tail;
      while (t != head)
         {
         if (++t >= max_buffers)
            t = 0;
         AudioStream::release(queue[t]);
         }
      tail = t;
   }

   int16_t* queueReadBuffer(void)  {  // W5BAA_INTERFACE only
      uint32_t t;
      if (userblock)
         return NULL;
      t = tail;
      if (t == head)
         return NULL;
      if (++t >= max_buffers)
         t = 0;
      userblock = queue[t];
      tail = t;
      return userblock->data;  // Pointer to 128 int16_t of data
   }

   void queueFreeBuffer(void)  {  // W5BAA_INTERFACE only
      if (userblock == NULL)
         return;
      AudioStream::release(userblock);
      userblock = NULL;
      }
#else
   // Regular 512/2048 float interface

   // Returns true when output data is available.
   bool available() {
      if (outputFlag == true)
         {
         outputFlag = false;  // No double returns
         return true;
         }
      return false;
      }

   // Start a new 14.7 sec data gather
   void startDataCollect(void)  {
      gettingData = true;
      FFTCount = 0;
      dec1Count = 0;
      dec2Count = 0;
      kOffset1 = 0;
      kOffset2 = 0;
      outputFlag = false;
      current128Used1 = false;
      current128Used2 = false;
      FFTCount = 0;
      FFTOld = 0;
      block128Count = 0;
      index2 = 0;        // Runs 0,511 on outputs
      }

   // Cancel the data gather
   void cancelDataCollect(void)  {
      gettingData = false;
      // Getting started again from here is by startDataCollect()
      }

   bool receivingData(void)  {
      return gettingData;
      }

   // Return FFT Count, 0 = No data
   //             1 to 184 = current 1024 words last returned
   // The number is incremented every 80 mSec during receive
   // Reset to 0 is by startDataCollect()
   int getFFTCount(void)  {
      return FFTCount;
      }

   float32_t* getDataPtr(void)  {  // Location of input for FFT
      return &data2K[0];
      }
#endif

   virtual void update(void);

private:
   audio_block_f32_t *inputQueueArray_f32[1];
   float sampleRateHz = AUDIO_SAMPLE_RATE;

   int16_t   srIndex = SR_NONE;
   //uint16_t block_size = 128;
   int16_t fcurrentArray = -1;
   float32_t *p0 = NULL;         // Pointers to 1024 storage
   float32_t *p1 = NULL;

uint32_t ttt;
int32_t kkk;

   bool FT8DemodInit = false;

   bool outputFlag = false;
   bool gettingData = false;
   bool current128Used1 = false;  // Decimation by 5
   bool current128Used2 = false;  // Decimation by 3
   audio_block_f32_t *inputQueueArray[1];
   int32_t  dec1Count = 0;
   int32_t  dec2Count = 0;
   float32_t dataIn[128];

   float32_t inFIR1[128];     // Inputs for FIR1
   // Working space for 55-tap FIR, 128 data points
   float32_t dec1FIRWork[183];
   float32_t outFIR1[128];
   int kOffset1 = 0;

   float32_t inFIR2[128];
   // Working space for 167-tap FIR
   float32_t dec2FIRWork[295];
   float32_t outFIR2[128];
   int kOffset2 = 0;
   arm_fir_instance_f32  fir_inst1, fir_inst2;

   bool powAvail = false;
   float32_t powerOut = 0.0f;
   float32_t outData[128];       // Storage after decimate by 3
#ifdef W5BAA_INTERFACE
	static const int max_buffers = 48;  // Enough for 3 FFT, i.e., plenty
	audio_block_t* volatile queue[max_buffers];
	audio_block_t* userblock;
	volatile uint8_t head = 0;
   volatile uint8_t tail = 0;
   volatile uint8_t enabled = 0;

	audio_block_t *block;
	uint32_t h;
#else
   int16_t FFTCount = 0;
   int FFTOld = 0;
   int16_t block128Count = 0;
   int16_t index2 = 0;        // Runs 0,511 on outputs
   float32_t data2K[2048];    // Output time array to FFT 2048+512
#endif


//   int64_t   sampleCount = 0LL;
   // bool      sampleCountOdd = false;

/* FOR INFO ONLY
    // Blackman-Harris produces a first sidelobe more than 90 dB down.
    // The price is a bandwidth of about 2 bins.  Very useful at times.
    void useBHWindow(void) {
        for (int i=0; i < 2048; i++) {
           float kx = 0.00306946f;  // 2*PI/2047
           int ix = (float) i;
           window[i] = 0.35875 -
                       0.48829*cosf(     kx*ix) +
                       0.14128*cosf(2.0f*kx*ix) -
                       0.01168*cosf(3.0f*kx*ix);
        }
    }
*/

/* FIR filter designed with http://t-filter.appspot.com
 * Sampling frequency = 96000 Hz
 * 0 Hz - 3200 Hz  ripple = 0.065 dB
 * 9600 Hz - 48000 Hz att = -81.1 dB   */
float32_t firDecimate1[55] = {      // constexpr static
 0.000037640f,  0.000248621f,  0.000535682f,  0.001017563f,  0.001647298,
 0.002359693f,  0.003009942f,  0.003388980f,  0.003245855f,  0.002335195,
 0.000482309f, -0.002343975f, -0.005965316f, -0.009953093f, -0.013627779f,
-0.016110493f, -0.016426909f, -0.013654288f, -0.007090645f,  0.003582981f,
 0.018178902f,  0.035947300f,  0.055606527f,  0.075464728f,  0.093619230f,
 0.108205411f,  0.117656340f,  0.120928651f,  0.117656340f,  0.108205411f,
 0.093619230f,  0.075464728f,  0.055606527f,  0.035947300f,  0.018178902f,
 0.003582981f, -0.007090645f, -0.013654288f, -0.016426909f, -0.016110493f,
-0.013627779f, -0.009953093f, -0.005965316f, -0.002343975f,  0.000482309f,
 0.002335195f,  0.003245855f,  0.003388980f,  0.003009942f,  0.002359693f,
 0.001647298f,  0.001017563f,  0.000535682f,  0.000248621f,  0.000037640f};

/* FIR filter designed with http://t-filter.appspot.com
 * Sampling frequency = 19200 Hz
 * 0 Hz - 2800 Hz ripple = 0.073 dB
 * 3200 Hz - 9600 Hz att = -80.0 dB  */
float32_t firDecimate2[167] = {
 0.000200074f,  0.000438821f,  0.000648425f,  0.000636175f,  0.000315069f,
-0.000193500f, -0.000591064f, -0.000600896f, -0.000202482f,  0.000301473f,
 0.000486276f,  0.000152104f, -0.000466930f, -0.000846593f, -0.000601871f,
 0.000140985f,  0.000780434f,  0.000717692f, -0.000092419f, -0.001015260f,
-0.001218073f, -0.000407595f,  0.000820258f,  0.001400419f,  0.000712814f,
-0.000783775f, -0.001824384f, -0.001392096f,  0.000312681f,  0.001894843f,
 0.001890948f,  0.000105692f, -0.002046807f, -0.002639347f, -0.000937478f,
 0.001779635f,  0.003148244f,  0.001757989f, -0.001441273f, -0.003731905f,
-0.002912877f,  0.000623820f,  0.003953823f,  0.004009904f,  0.000373714f,
-0.004041709f, -0.005284541f, -0.001864153f,  0.003610036f,  0.006359221f,
 0.003566059f, -0.002829631f, -0.007374880f, -0.005699305f,  0.001364774f,
 0.007956768f,  0.007979137f,  0.000650460f, -0.008163693f, -0.010542154f,
-0.003518210f,  0.007604450f,  0.013094981f,  0.007160556f, -0.006233941f,
-0.015716023f, -0.011940068f,  0.003541776f,  0.018116367f,  0.018029298f,
 0.000861510f, -0.020344029f, -0.026351929f, -0.008277630f,  0.022137232f,
 0.038780109f,  0.021608602f, -0.023552791f, -0.062590967f, -0.053225138f,
 0.024375124f,  0.148263262f,  0.262405223f,  0.308632386f,  0.262405223f,
 0.148263262f,  0.024375124f, -0.053225138f, -0.062590967f, -0.023552791f,
 0.021608602f,  0.038780109f,  0.022137232f, -0.008277630f, -0.026351929f,
-0.020344029f,  0.000861510f,  0.018029298f,  0.018116367f,  0.003541776f,
-0.011940068f, -0.015716023f, -0.006233941f,  0.007160556f,  0.013094981f,
 0.007604450f, -0.003518210f, -0.010542154f, -0.008163693f,  0.000650460f,
 0.007979137f,  0.007956768f,  0.001364774f, -0.005699305f, -0.007374880f,
-0.002829631f,  0.003566059f,  0.006359221f,  0.003610036f, -0.001864153f,
-0.005284541f, -0.004041709f,  0.000373714f,  0.004009904f,  0.003953823f,
 0.000623820f, -0.002912877f, -0.003731905f, -0.001441273f,  0.001757989f,
 0.003148244f,  0.001779635f, -0.000937478f, -0.002639347f, -0.002046807f,
 0.000105692f,  0.001890948f,  0.001894843f,  0.000312681f, -0.001392096f,
-0.001824384f, -0.000783775f,  0.000712814f,  0.001400419f,  0.000820258f,
-0.000407595f, -0.001218073f, -0.001015260f, -0.000092419f,  0.000717692f,
 0.000780434f,  0.000140985f, -0.000601871f, -0.000846593f, -0.000466930f,
 0.000152104f,  0.000486276f,  0.000301473f, -0.000202482f, -0.000600896f,
-0.000591064f, -0.000193500f,  0.000315069f,  0.000636175f,  0.000648425f,
 0.000438821f,  0.000200074f};

};    // End class def

// endif for Teensy 4.x only:
#endif

// endif for single read of .h file:
#endif
