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
 * rates become 11.025, 12, 22, 24, 44.1, 48, 50, 88, 96 and 100 ksps or others
 * in those general ranges.
 */
// Revised 25 Aug 2025. Added manual keying capability with Gaussian filtering.
// Corrected peration at sampling rates above 12 ksps.   Bob L
// Revised 29 Aug 2025 to extend lower range of sampling rate and to include
// 22 to 25 ksps.  setSampleRate_Hz() changed to bool (true if successful, false
// if unsupported rate.

// *** 15 SEPT 2025  VERSION USING FULL FIR FILTERS (multiplying by zeros) ***

/* ToDo:  Revise to polyphase interpolation to improve execution time.
   See Richard G. Lyons,  "Understanding Digital Signal Processing,"
   Third Edition, Prentice-Hall 2011, Section 10.12.2 Half-Band
   Implementations, Figure 10-27(d). This procedure alternates half-length
   FIR Filters with single term multiplys by 0.5.

   The padding of the input array with zeros is accomplished as part of
   the FIR LP filtering by designing the filter to be symmetrical about
   Fsample/4, along with an odd number of coefficients. Then every other
   filter coefficient is zero, except for the center one that is
   always 0.5. The zero-product multiply-and-accumulates are not done by
   omitting them from the coefficient array.
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

#define SR_12KSPS  0
#define SR_24KSPS  1
#define SR_48KSPS  2
#define SR_96KSPS  3

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
      arm_fir_interpolate_init_f32 (&interp12_24Inst, 2, 40, b38A, pState12_24, 64);
      arm_fir_interpolate_init_f32 (&interp24_48Inst, 2, 40, b38A, pState24_48, 64);
      arm_fir_interpolate_init_f32 (&interp48_96Inst, 2, 40, b38A, pState48_96, 64);
      setSampleRate_Hz(sample_rate_Hz);
      setFrequency(600.0f);
      // Clear interpolation buffers
      for(int ii=0; ii<64; ii++)
         dataBuf12[ii] = 0.0f;
      for(int ii=0; ii<128; ii++)
         {
         dataBuf12[ii]     = 0.0f;
         dataBuf24[ii] = 0.0f;    // Buffer for 12 to 24 ksps interp
         dataBuf48[ii] = 0.0f;
         dataBuf96[ii] = 0.0f;
         }
      }

   // Enables the operation of the update().  This is needed when there
   // are separate transmit and receive times.
   void enableTransmit(bool _transmit)  {
      enableXmit = _transmit;
      }

   bool getEnableTransmit(void)  {
       return enableXmit;
   }

   // Enables/disables manual keying. Buffer sent keying is disabled.  The
   // Gaussian keying shaping is continued.  Buffers are left untouched
   // when manual CW is enabled.
   void enableManualCW(bool _manualCW)  {
      manualCW = _manualCW;
      }

   bool getmanualCW(void)  {
       return manualCW;
   }

   void manualCWKey(bool _keyDown)  {
	   keyDown = _keyDown;
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
   bool setSampleRate_Hz (float32_t fs_Hz)  {
      bool returnState = true;
      sample_rate_Hz = fs_Hz;
      setFrequency(frequencyHz);
      phaseIncrement = 512.0f*frequencyHz/sample_rate_Hz;
      if(sample_rate_Hz>10900.0f && sample_rate_Hz<12100.0f)
         {
         nSample = 1;
         nSamplesPerUpdate = 128;
         sampleRate = SR_12KSPS;
         }
      else if(sample_rate_Hz>21900.0f && sample_rate_Hz<25100.0f)
         {
         nSample = 2;
         nSamplesPerUpdate = 64;
         sampleRate = SR_24KSPS;
         }
      else if(sample_rate_Hz>43900.0f && sample_rate_Hz<50100.0f)
         {
         nSample = 4;
         nSamplesPerUpdate = 32;
         sampleRate = SR_48KSPS;
         }
      else if(sample_rate_Hz>87900.0f && sample_rate_Hz<100100.0f)
         {
         nSample = 8;
         nSamplesPerUpdate = 16;
         sampleRate = SR_96KSPS;
         }
      else
         {
         nSample = 4;
         nSamplesPerUpdate = 32;
         sampleRate = SR_48KSPS;
         setFrequency(48000.0f);
         phaseIncrement = 512.0f*frequencyHz/sample_rate_Hz;
         returnState = false;
         }
      arm_fir_init_f32(&GaussLPFInst, 145, &firGaussCoeffs[0], &GaussState[0], nSamplesPerUpdate);
      setFrequency(frequencyHz);
      return returnState;
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
   uint8_t   sendBuffer[512];    // Circular send buffer for CW chars
   uint16_t  indexW = 0;         // Write index in to buffer
   uint16_t  indexR = 0;         // Read index into buffer
   float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;
   int       block_size = 128;
   int       sampleRate = SR_48KSPS;
   int       nSample = 4;
   int       nSamplesPerUpdate = 32;
   uint16_t  stateCW = IDLE_CW;
   uint8_t   c = 0;
   uint16_t  ic = 0;           // Current character as integer
   bool enableXmit = true;
   bool manualCW = false;
   bool keyDown = false;
   float32_t levelCW = 0.0f;   // sample by sample
   float32_t frequencyHz = 600.0f;
   float32_t phaseIncrement = 512.0f*frequencyHz/sample_rate_Hz;
   float32_t phaseS = 0.0f;
   float32_t magnitude = 1.0f;
   float32_t timeMsF = 0.0f;   // Update the event clock, as float
   uint32_t  timeMsI = 0;      // This int is easier to use
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

   arm_fir_interpolate_instance_f32 interp12_24Inst;
   float32_t  pState12_24[148];

   arm_fir_interpolate_instance_f32 interp24_48Inst;
   float32_t  pState24_48[148];

   arm_fir_interpolate_instance_f32 interp48_96Inst;
   float32_t  pState48_96[148];

   // Circular buffers for interpolation
   float32_t  dataBuf12[128];  // Buffer for 12 kHz signal start
   float32_t  dataBuf12A[64]; // Temp storage after Gaussian LPF, if SR>12
   float32_t  dataBuf24[128];  // Buffer for 12 to 24 ksps interp
   float32_t  dataBuf48[128];  // Buffer for 24 to 48 ksps interp
   float32_t  dataBuf96[128];  // Buffer for 48 to 96 ksps interp

// For first stage of x2 interpolation.  SampleFreq is the new x2 rate.
// b38 coefficients 0.003dB pass band up to 0.2*SampleFreq, -70 dB stop band above 0.3*SampleFreq
// Includes a gain of 2.0
/*float32_t b38[20] = {-0.0012772, 0.003261, -0.0070986, 0.0135458,
                     -0.023764,  0.039562, -0.064408,  0.107022,
                     -0.199250,  0.632406,  0.632408, -0.19925,
                      0.107022, -0.064408,  0.039562, -0.023764,
                      0.0135458,-0.0070986, 0.003261, -0.0012772};
 */
// float32_t b38mid = 0.500;  Always 0.5 and built into functions
// The original filter with zeros left in.
float32_t b38A[40] = {-0.0006386, 0.0, 0.0016305,  0.0, -0.0035493, 0.0,
                       0.0067729, 0.0, -0.011882,  0.0,  0.019781,  0.0,
                      -0.032204,  0.0,  0.053511,  0.0, -0.099725,  0.0,
                       0.31613,   0.5,  0.31613,   0.0, -0.099725,  0.0,
                       0.053511,  0.0, -0.032204,  0.0,  0.019781,  0.0,
                      -0.011882,  0.0,  0.0067729, 0.0, -0.0035493, 0.0,
                       0.0016305, 0.0, -0.0006386, 0.0};

/*
// For second or third stage x2 interpolation
// b38 coefficients 0.002dB pass band up to 0.125*SampleFreq, -72 dB stop band above 0.375*SampleFreq
float32_t b14[8] = {-3.7369195e-03,  2.0567858e-02, -7.2319757e-02,  3.0536909e-01,
                     3.0536909e-01, -7.2319757e-02,  2.0567858e-02, -3.7369195e-03};
// float32_t b14mid = 0.500; Always 0.5 and built into functions
// The original filter with zeros left in:
float32_t b14A[16] = {-3.7369195e-03, 0.0, 2.0567858e-02,  0.0, -7.2319757e-02, 0.0,
                       3.0536909e-01, 0.5, 3.0536909e-01,  0.0, -7.2319757e-02, 0.0,
                       2.0567858e-02, 0.0, -3.7369195e-03, 0.0};
 */

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

// CW LPF Gaussian 12 ksps, 
float32_t coeffLPFGaussCW70[145] =  {
 0.000000000013f,  0.000000000230f,  0.000000001967f, 0.000000011200f,  0.000000048141f,
 0.000000167593f,  0.000000494741f,  0.000001279313f, 0.000002968163f,  0.000006293182f,
 0.000012368634f,  0.000022789991f,  0.000039725258f, 0.000065989656f,  0.000105095272f,
 0.000161268735f,  0.000239431946f,  0.000345143158f, 0.000484498100f,  0.000663993182f,
 0.000890354951f,  0.001170341805f,  0.001510525404f, 0.001917060227f,  0.002395450260f,
 0.002950321908f,  0.003585211904f,  0.004302378311f, 0.005102641686f,  0.005985262283f,
 0.006947857700f,  0.007986363895f,  0.009095040924f, 0.010266523229f,  0.011491912843f,
 0.012760912591f,  0.014061995183f,  0.015382603150f, 0.016709373840f,  0.018028383143f,
 0.019325401352f,  0.020586154438f,  0.021796584180f, 0.022943100872f,  0.024012822795f,
 0.024993797252f,  0.025875198659f,  0.026647500006f, 0.027302614782f,  0.027834007382f,
 0.028236770835f,  0.028507671540f,  0.028645161477f, 0.028649359097f,  0.028522000726f,
 0.028266364893f,  0.027887172428f,  0.027390465579f, 0.026783469604f,  0.026074440512f,
 0.025272502642f,  0.024387479779f,  0.023429723379f, 0.022409941313f,  0.021339030279f,
 0.020227914764f,  0.019087395070f,  0.017928006598f, 0.016759892161f,  0.015592688743f,
 0.014435429706f,  0.013296463085f,  0.012183386246f, 0.011102996841f,  0.010061259692f,
 0.009063288961f,  0.008113344718f,  0.007214842832f, 0.006370376935f,  0.005581751105f,
 0.004850021809f,  0.004175547619f,  0.003558045198f, 0.002996650053f,  0.002489980622f,
 0.002036204323f,  0.001633104269f,  0.001278145483f, 0.000968539557f,  0.000701306812f,
 0.000473335187f,  0.000281435183f,  0.000122390339f, -0.000006997132f, -0.000109865828f,
-0.000189259903f, -0.000248099099f, -0.000289154009f, -0.000315026421f, -0.000328134503f,
-0.000330702557f, -0.000324754989f, -0.000312114118f, -0.000294401420f, -0.000273041783f,
-0.000249270341f, -0.000224141464f, -0.000198539480f, -0.000173190738f, -0.000148676626f,
-0.000125447205f, -0.000103835123f, -0.000084069549f, -0.000066289850f, -0.000050558825f,
-0.000036875286f, -0.000025185874f, -0.000015395980f, -0.000007379693f, -0.000000988737f,
 0.000003939638f,  0.000007575807f,  0.000010091935f, 0.000011657137f,  0.000012433551f,
 0.000012573298f,  0.000012216214f,  0.000011488311f, 0.000010500866f,  0.000009350060f,
 0.000008117080f,  0.000006868609f,  0.000005657627f, 0.000004524445f,  0.000003497908f,
 0.000002596712f,  0.000001830778f,  0.000001202630f, 0.000000708753f,  0.000000340886f,
 0.000000087231f, -0.000000066451f, -0.000000135848f, -0.000000137250f, -0.000000086820f}; 
};
#endif
