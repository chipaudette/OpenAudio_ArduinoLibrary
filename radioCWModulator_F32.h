/*
 * radioCWModulator_F32.h   For the OpenAudio_TeensyArduino library
 * (floating point audio).
 *   MIT License - Copyright March 2023  Bob Larkin, original write
 *
 * Parts of this are based on Audio Library for Teensy 3.x and 4.x that is
 * Copyright (c) 2023, Paul Stoffregen, paul@pjrc.com
 *
 * Development of the Teensy audio library was funded by PJRC.COM, LLC by sales of
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
/* The method comes directly from the UHFA program for the DSP-10 project of 1999
 * by Bob Larkin.  Much of the code comes from there, as well.
 *
 * Sample rates:  The CW is generated at a sample rate of 12 ksps. This
 * supports a good Gaussian filter to limit the spectral width which is
 * equivalent to removing "key clicks."  To use higher sample rates, the
 * generation is only done for every 2nd, 4th or 8th sample.  The output
 * for sample rates above 12 ksps are interpolated.  The supported sample
 * rates become 11.025, 12, 44.1, 48, 50, 88, 96 and 100 ksps or others
 * in those general ranges.
 */


#ifndef radioCWModulator_F32_h_
#define radioCWModulator_F32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"

class radioCWModulator_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:DetCTCSS
public:
   radioCWModulator_F32(void) : AudioStream_F32(0, NULL) {
      sample_rate_Hz = AUDIO_SAMPLE_RATE;
      block_size = AUDIO_BLOCK_SAMPLES;
      initCW();
      }
   // Option of AudioSettings_F32 change sample rate (always 128 block size):
   radioCWModulator_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL) {
      sample_rate_Hz = settings.sample_rate_Hz;
      block_size = AUDIO_BLOCK_SAMPLES;
      initCW();
      }

// Values for stateCW
#define IDLE_CW   0
#define DASH_CW   1
#define DOT_CW    2
#define DOT_DASH_SPACE 3
#define CHAR_SPACE     4
#define WORD_SPACE     5
#define LONG_DASH      6

   void initCW(void)  {   //  Public, but primarily for building objects.  Not required in INO.
      for(int kk=0; kk<512; kk++)
         sendBuffer[kk] = 0;
      indexW = 0;
      indexR = 0;
      setCWSpeedWPM(speedWPM);
      arm_fir_init_f32(&interpolateLPFInst, 59, &interpolateLPF[0], &interpolateLPFState[0], 128);
      setSampleRate_Hz(sample_rate_Hz);
      setFrequency(600.0f);
      }

   // Enables the operation of the update().  This is needed when there
   // are separate transmit and receive times.
   void enableTransmit(bool _transmit)  {
      enableXmit = _transmit;
      }

   bool getEnableTransmit(void)  {
       return enableXmit;
   }

   uint16_t getBufferSpace(void)  {
      return 512 - ((indexW - indexR) & 0X1FF);
      }

   // Place character into queue for sending
   bool sendCW(uint16_t _ch)  {
      if(getBufferSpace() > 0)
         {
         sendBuffer[indexW++] = _ch;
         indexW &= 0X1FF;
         return true;
         }
      else
         return false;
      }

   bool sendStringCW(char* _pStr)
      {
      uint16_t space = getBufferSpace();
      uint16_t size = strlen(_pStr);
      // Serial.print(space); Serial.print(" space    size "); Serial.println(size);
      if(space < size)  return  false;
      for(int kk=0; kk<(int)strlen(_pStr); kk++)
         sendCW( (uint16_t)*(_pStr+kk) );
      return true;
      }

   void setCWSpeedWPM(uint16_t _speed)  {
      uint16_t kk = 0;
      while(tCW[kk].speedWPM < _speed)
         kk++;
      speedIndex = kk;
      dotCW =  (uint32_t)tCW[kk].dot;       /* msec per dot */
      dashCW = (uint32_t)tCW[kk].dash;      /* msec per dash */
      ddCW =   (uint32_t)tCW[kk].dd;        /* msec between dot or dash */
      chCW =   (uint32_t)tCW[kk].ch;        /* msec at end of each char */
      spCW =   (uint32_t)tCW[kk].sp;        /* msec for space char */
      }

   void setFrequency(float32_t _freq) {      // Defaults to 600 Hz
      frequencyHz = _freq;
      if (frequencyHz < 0.0f)
         frequencyHz = 0.0f;
      if (frequencyHz > sample_rate_Hz/2.0f)
         frequencyHz = sample_rate_Hz/2.0f;
      phaseIncrement = 512.0f * frequencyHz / sample_rate_Hz;
      }


   void setLongDashMs(uint32_t _longDashMs)  {    // Defaults to 1000 mSec
      longDashCW = _longDashMs;
      }

   // See note above on sample rates.
   void setSampleRate_Hz (float32_t fs_Hz)  {             // (const float32_t &fs_Hz) {
      sample_rate_Hz = fs_Hz;
      timeSamplesMs =  1000.0f/sample_rate_Hz;     // In millisec
      if(sample_rate_Hz>11000.0f && sample_rate_Hz<12100.0f)
         {
         nSample = 1;
         nSamplesPerUpdate = 128;
         }
      else if(sample_rate_Hz>44000.0f && sample_rate_Hz<50100.0f)
         {
         nSample = 4;
         nSamplesPerUpdate = 32;
         }
      else if(sample_rate_Hz>88000.0f && sample_rate_Hz<100100.0f)
         {
         nSample = 8;
         nSamplesPerUpdate = 16;
         }
      else
         {
         Serial.print(sample_rate_Hz);
         Serial.println(" sample rate not supported.");
         nSample = 4;
         nSamplesPerUpdate = 32;
         }
      arm_fir_init_f32(&GaussLPFInst, 145, &firGaussCoeffs[0], &GaussState[0], nSamplesPerUpdate);
      setFrequency(frequencyHz);
      }

   // The amplitude, a, is the peak, as in zero-to-peak.  This produces outputs
   // ranging from -a to +a.
   void amplitude(float32_t _a) {
      if (_a < 0.0f)
         _a = 0.0f;
      magnitude = _a;
      }

   virtual void update(void);

private:
   uint8_t   sendBuffer[512];    // Circular send buffer
   uint16_t  indexW = 0;         // Write index in to buffer
   uint16_t  indexR = 0;         // Read index into buffer
   float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;
   int16_t   block_size = 128;
   int16_t   nSample = 4;        //
   int16_t   nSamplesPerUpdate = 32;
   uint16_t  stateCW = IDLE_CW;
   uint8_t   c = 0;
   uint16_t  ic = 0;           // Current character as integer
   bool enableXmit = true;
   float32_t levelCW = 0.0f;   // sample by sample
   float32_t frequencyHz = 600.0f;
   float32_t phaseIncrement = 512.0f*frequencyHz/sample_rate_Hz;
   float32_t phaseS = 0.0f;
   float32_t magnitude = 1.0f;

   float32_t timeSamplesMs =  1000.0f/sample_rate_Hz;     // In millisec
   float32_t timeMsF = 0.0f;   // Update the event clock
   uint32_t  timeMsI = 0;      // This is easier to use
   uint16_t  speedWPM = 13;
   uint16_t  speedIndex = 8;

   uint32_t  dotCW;        /* msec per dot */
   uint32_t  dashCW;       /* msec per dash */
   uint32_t  ddCW;         /* msec between dot or dash */
   uint32_t  chCW;         /* msec at end of each char */
   uint32_t  spCW;         /* msec for space char */
   uint32_t  longDashCW = 1000;

   float32_t* firGaussCoeffs = coeffLPFGaussCW70;
   arm_fir_instance_f32 GaussLPFInst;
   float32_t GaussState[128 + 145];

   arm_fir_instance_f32 interpolateLPFInst;
   float32_t interpolateLPFState[128 + 59];

   /* The following mc[] table defines the morse code sequences
    * in terms of bits. Starting from lsb and continuing until only one
    * "1" remains, send a dot for a "0" and a dash for a "1", with
    * a right shift after each dot or dash.  */

   /*                    sp         "         $              '    */
const uint16_t  mc[64] = {0x17,0x01,0x52,0x01,0xC8,0x01,0x01,0x5E,

   /*                     (    )              ,    -    .    /    */
                        0x2D,0x6D,0x01,0x01,0x73,0x61,0x6A,0x29,

   /*                     0    1    2    3    4    5    6    7    */
                        0x3f,0x3e,0x3c,0x38,0x30,0x20,0x21,0x23,

   /*  ; = long dash      8    9    :    ;         =   KN    ?    */
                        0x27,0x2f,0x47,0x01,0x01,0x31,0x2D,0x4C,

   /*                     @    A    B    C    D    E    F    G    */
                        0x5a,0x06,0x11,0x15,0x09,0x02,0x14,0x0b,

   /*                     H    I    J    K    L    M    N    O    */
                        0x10,0x04,0x1e,0x0d,0x12,0x07,0x05,0x0f,

   /*                     P    Q    R    S    T    U    V    W    */
                        0x16,0x1b,0x0a,0x08,0x03,0x0c,0x18,0x0e,

   /*                     X    Y    Z   AR   AS   SK   Err   _    */
                        0x19,0x1d,0x13,0x2A,0x22,0x68,0x80,0x6C};

/* The following set the weightings vs. code speed in wpm
 * by selecting the delays for
 * .dot   length of a dot in ms
 * .dash  length of a dash in ms
 * .dd    time between dots and dashes, ms
 * .ch    time between characters of the same word, ms
 * .sp    time duration between words, ms     */
struct cw_data {
   uint16_t  speedWPM;
   uint16_t  dot;        /* msec per dot */
   uint16_t  dash;       /* msec per dash */
   uint16_t  dd;         /* msec between dot or dash */
   uint16_t  ch;         /* msec at end of each char */
   uint16_t  sp;         /* msec for space char */
   };
/* Note:  Code speed can be any.  There are 5 times in milliseconds that
 * set the actual speed and weighting.  The index (0, 28) allows 29 of these
 * that are initialized to 5 WPM to 50 WPM with nominal weightings.   */
struct cw_data tCW[29] = {
  {  5, 150, 530, 150, 900,1200},
  {  6, 140, 480, 140, 750,1000},
  {  7, 125, 430, 125, 550, 800},
  {  8, 120, 400, 120, 400, 725},
  {  9, 120, 380, 120, 300, 560},
  { 10, 120, 360, 120, 240, 480},
  { 11, 110, 330, 110, 220, 440},
  { 12, 100, 300, 100, 200, 400},
  { 13,  92, 276,  92, 184, 368},
  { 14,  86, 258,  86, 172, 344},
  { 15,  80, 240,  80, 160, 320},
  { 16,  75, 225,  75, 150, 300},
  { 17,  70, 212,  70, 142, 283},
  { 18,  66, 200,  66, 134, 267},
  { 19,  63, 189,  63, 126, 252},
  { 20,  60, 180,  60, 120, 240},
  { 21,  57, 171,  57, 114, 230},
  { 22,  55, 164,  55, 109, 220},
  { 23,  52, 157,  52, 105, 210},
  { 24,  50, 150,  50, 100, 200},
  { 25,  48, 144,  48,  96, 192},
  { 26,  47, 138,  47,  91, 185},
  { 27,  45, 133,  45,  88, 179},
  { 28,  43, 129,  43,  86, 172},
  { 29,  41, 124,  41,  83, 166},
  { 30,  40, 120,  40,  80, 160},
  { 35,  34, 103,  34,  69, 138},
  { 40,  30,  90,  30,  60, 120},
  { 50,  24,  72,  24,  48,  96}};

// CW LPF Gaussian 12 ksps, 70 Hz at 3 dB
float32_t coeffLPFGaussCW70[145] =  {
 0.0000000994f, 0.0000002465f, 0.0000005437f, 0.0000010924f, 0.0000020348f,
 0.0000035609f, 0.0000059142f, 0.0000093971f, 0.0000143746f, 0.0000212771f,
 0.0000306017f, 0.0000429118f, 0.0000588363f, 0.0000790660f, 0.0001043500f,
 0.0001354903f, 0.0001733351f, 0.0002187713f, 0.0002727164f, 0.0003361089f,
 0.0004098986f, 0.0004950364f, 0.0005924635f, 0.0007031003f, 0.0008278357f,
 0.0009675161f, 0.0011229345f, 0.0012948202f, 0.0014838290f, 0.0016905333f,
 0.0019154139f, 0.0021588516f, 0.0024211200f, 0.0027023793f, 0.0030026708f,
 0.0033219125f, 0.0036598959f, 0.0040162835f, 0.0043906075f, 0.0047822700f,
 0.0051905436f, 0.0056145733f, 0.0060533799f, 0.0065058635f, 0.0069708087f,
 0.0074468905f, 0.0079326809f, 0.0084266565f, 0.0089272066f, 0.0094326423f,
 0.0099412055f, 0.0104510798f, 0.0109603999f, 0.0114672630f, 0.0119697396f,
 0.0124658847f, 0.0129537493f, 0.0134313915f, 0.0138968878f, 0.0143483444f,
 0.0147839080f, 0.0152017763f, 0.0156002085f, 0.0159775353f, 0.0163321678f,
 0.0166626067f, 0.0169674505f, 0.0172454031f, 0.0174952809f, 0.0177160188f,
 0.0179066758f, 0.0180664400f, 0.0181946320f, 0.0182907087f, 0.0183542653f,
 0.0183850369f, 0.0183828991f, 0.0183478684f, 0.0182801008f, 0.0181798905f,
 0.0180476675f, 0.0178839942f, 0.0176895623f, 0.0174651874f, 0.0172118049f,
 0.0169304634f, 0.0166223192f, 0.0162886292f, 0.0159307439f, 0.0155500995f,
 0.0151482106f, 0.0147266612f, 0.0142870968f, 0.0138312154f, 0.0133607587f,
 0.0128775032f, 0.0123832512f, 0.0118798218f, 0.0113690419f, 0.0108527375f,
 0.0103327249f, 0.0098108024f, 0.0092887417f, 0.0087682803f, 0.0082511135f,
 0.0077388875f, 0.0072331919f, 0.0067355538f, 0.0062474314f, 0.0057702081f,
 0.0053051882f, 0.0048535913f, 0.0044165491f, 0.0039951010f, 0.0035901916f,
 0.0032026682f, 0.0028332785f, 0.0024826695f, 0.0021513865f, 0.0018398728f,
 0.0015484699f, 0.0012774182f, 0.0010268582f, 0.0007968321f, 0.0005872861f,
 0.0003980729f, 0.0002289548f, 0.0000796069f,-0.0000503789f,-0.0001614900f,
-0.0002542886f,-0.0003294068f,-0.0003875421f,-0.0004294517f,-0.0004559477f,
-0.0004678910f,-0.0004661861f,-0.0004517752f,-0.0004256325f,-0.0003887583f,
-0.0003421730f,-0.0002869118f,-0.0002240186f,-0.0001545404f,-0.0000795219f};

/* FIR filter designed with http://t-filter.appspot.com
Fs = 96000 Hz  [48000]
    0 Hz to 4000 Hz  [0 to 2000]     actual ripple = 0.05 dB
11000 Hz to 48000 Hz [5500 to 24000] actual atten = -93.1 dB  */
float32_t interpolateLPF[59] = {
-0.000052355f,-0.000146720f,-0.000307316f,-0.000517033f,-0.000721194f,-0.000819999f,
-0.000680277f,-0.000169005f, 0.000793689f, 0.002172809f, 0.003767656f, 0.005194993f,
 0.005927352f, 0.005395822f, 0.003149023f,-0.000960853f,-0.006611837f,-0.012919698f,
-0.018466308f,-0.021473003f,-0.020108212f,-0.012880252f, 0.000965492f, 0.021140621f,
 0.046185831f, 0.073574790f, 0.100053429f, 0.122161788f, 0.136838160f, 0.141979757f,
 0.136838160f, 0.122161788f, 0.100053429f, 0.073574790f, 0.046185831f, 0.021140621f,

0.000965492f,-0.012880252f,-0.020108212f,-0.021473003f,-0.018466308f,-0.012919698f,
-0.006611837f,-0.000960853f, 0.003149023f, 0.005395822f, 0.005927352f, 0.005194993f,
 0.003767656f, 0.002172809f, 0.000793689f,-0.000169005f,-0.000680277f,-0.000819999f,
-0.000721194f,-0.000517033f,-0.000307316f,-0.000146720f,-0.000052355f};

};
#endif
