/*
 * radioFT8modulator_F32.h
 *
 * FT-8 Modulator including Gaussian frequency change filtering
 * and raised cosine amplitude control to limit spectral width.
 * By Bob Larkin  W7PUA   16 July 2022
 *
 * Thanks to PJRC and Pau Stoffregen for the Teensy processor, Teensyduino
 * and Teensy Audio.  Thanks to Chip Audette for the F32 extension
 * work.  All of that makes this possible.  Bob
 *
 * Copyright (c) 2022 Bob Larkin
 * Portions Copyright (c) 2018 Kārlis Goba (see below)
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
 *
 * *** CREDITS ***
 * There have been many contributors to this FT8 project.  I will not
 * catch them all, but a try is neeeded.  I picked from
 * multiple sources in making the transmission process consistent with the
 * 128-block Teensy Audio Library.  But, everything I did was a translation
 * of existing software.  The "specification" for FT8 is the paper, "The FT4
 * and FT8 Communications Protocols," by Steve Franke, K9AN, Bill Somerville,
 * G4WJS and Joe Taylor, K1JT, QEX July/August 2020, pp7-17.  I was guided
 * in the right direction for this implementation by Farhan, VU2ESE
 * and Charlie, W5BAA. This led to a pair of excellent Raspberry Pi sources,
 * https://github.com/kgoba/ft8_lib and
 * https://github.com/WB2CBA/W5BAA-FT8-POCKET-TERMINAL
 * An FT8 derivative of the first web site, using the Raspberry Pi,
 * is Farhan's sBitx:
 * https://github.com/afarhan/sbitx
 * For those not familiar with FT8, it is a widely used amateur radio
 * communications format.  A search of FT8 will lead to much information.
 *
 *Notes:
 * 1 - Sampling rate -  The output of this class is a sampled sine wave,
 * shifted in frequency according to the FT-8 specification. At this point,
 *  only 48 and 96 kHz sample rates are supported.
 */

/* 2 - Bit timing is simple for sample rate of 48 kHz where we start a new bit
 * every 60 128-sample period blocks, or 96 kHz where it is every 120
 * 128-sample period blocks.
 */

/* 3 - This class does not provide true (absolute) clock timing. The sendData() function
 * should be called when it is time for a new 15-sec period transmission.
 * This class will start that at the next 128 audio sample period.  Worst
 * case error is 128/sampleRate seconds, which is 2.667 milliseconds for
 * a 48 kHz sample rate.  Starting means producing audio output, as this
 * class does not control PTT.
 */

/* 4 - The bits are transmitted by 1 of 8 tones that change every 0.16 seconds.
 * To restrict the occupied spectrum, the frequency control signal is
 * filtered by a Gaussian LPF (see Franke, Somerville, Taylor QEX
 * July/Aug 2020).  There are 58 symbols carrying the data payload, CRC check,
 * and error correcting bits.  These are grouped into two 29 symbol words
 * MA and MB. There are 3 synchronization words, S, of 7 symbols
 * and two end amplitude ramp symbols, R1=3 and R2=2, that are only for
 * stopping key clicks.  All together there are 81 symbol periods of
 * 0.16 sec used as R1 S MA S MB S R2.  Transmission time is 81*0.16=12.96 sec.
 */

/* 5 - As explained in the QEX article, the control signal is not filtered
 * directly to get the Gaussian transitions.  Instead, to save computation,
 * Gaussian LP filtering is applied to a unit pulse 0.16 secon in time.
 * This resulting "pulse" can then be multiplied by the frequency
 * offset.  Linearity enables this simple sum.  This can be thought of as
 * having a base frequency.  Each pulse moves the frequency to its needed
 * value and then returns, after 0.16 sec, to the base frequency.  Without
 * delay the next pulse moves the frequency to some other value.  Instead of
 * ever going from 5 to 3, we go from 5 to 0 followed immediately by
 * going from 0 to 3.
 */

/* 6 - Time requirements - The update function works on 128 samples as a group.
 * Measured on a T4.0, that function required 19 microseconds, or less.  This
 * is about 0.7% of the available time at 48 kHz and 1.4% at 96 kHz, i.e.,
 * it is very small.
 */

/* MESSAGE FORMATS: The following table shows examples of message formats
supported by FT8 protocol.  Parameters i3 and n3
(shown in the first column) are used in the software to define major
and minor 77-bit message types.
* TODO: Mark the formats that are currently supported.
*
i3.n3  Example Messages       Comments
————————————————————————————————————————————————————-
0.0 TNX BOB 73 GL           Free text
0.1 K1ABC RR73; W9XYZ <KH1/KH7Z> -08   DXpedition Mode
0.2 PA9XYZ 590003 IO91NP    EU VHF Contest
0.2 G4ABC/P R 570007 JO22DB EU VHF Contest
0.3 K1ABC W9XYZ 6A WI       ARRL Field Day
0.3 W9XYZ K1ABC R 2B EMA    ARRL Field Day
0.5 123456789ABCDEF012      Telemetry (71 bits, 18 hex digits)
1. CQ FD K1ABC FN42         ARRL Field Day
1. CQ RU K1ABC FN42         ARRL RTTY Roundup
1. CQ K1ABC FN42
1. CQ TEST K1ABC FN42       NA VHF Contest
1. CQ TEST K1ABC/R FN42     NA VHF Contest
1. K1ABC W9XYZ EN37
1. K1ABC W9XYZ -09
1. K1ABC W9XYZ R-17
1. K1ABC W9XYZ RRR
1. K1ABC W9XYZ 73
1. K1ABC W9XYZ RR73
1. K1ABC/R W9XYZ EN37       NA VHF Contest
1. K1ABC W9XYZ/R RR73       NA VHF Contest
1. K1ABC/R W9XYZ/R RR73     NA VHF Contest
1. <PJ4/K1ABC> W9XYZ        Compound call
1. W9XYZ <PJ4/K1ABC> 73     Compound call
1. W9XYZ <YW18FIFA> -13     Nonstandard call
1. <YW18FIFA> W9XYZ R+02    Nonstandard call
1. W9XYZ <YW18FIFA> RRR     Nonstandard call
1. <YW18FIFA> W9XYZ RR73    Nonstandard call
2. CQ G4ABC/P IO91          EU VHF contest
2. G4ABC/P PA9XYZ JO22      EU VHF contest
2. PA9XYZ G4ABC/P RR73      EU VHF contest
3. K1ABC KA0DEF 559 MO      ARRL RTTY Roundup
3. K1ABC W9XYZ 579 WI       ARRL RTTY Roundup
3. KA1ABC G3AAA 529 0013    ARRL RTTY Roundup
3. TU; KA0DEF K1ABC R 569 MA   ARRL RTTY Roundup
3. TU; K1ABC G3AAA R 559 0194  ARRL RTTY Roundup
3. W9XYZ K1ABC R 589 MA     ARRL RTTY Roundup
4. CQ PJ4/K1ABC
4. CQ YW18FIFA              Nonstandard call
4. <KA1ABC> YW18FIFA RR73   Nonstandard call
4. <W9XYZ> PJ4/K1ABC RRR    Nonstandard call
4. <W9XYZ> YW18FIFA         Nonstandard call
4. <W9XYZ> YW18FIFA 73      Nonstandard call
4. PJ4/K1ABC <W9XYZ>        Nonstandard call
4. PJ4/K1ABC <W9XYZ> 73     Nonstandard call
4. YW18FIFA <W9XYZ> RRR     Nonstandard call
———————————————————————————-

In the above list, callsigns enclosed in angle brackets (e.g.,
<PJ4/K1ABC>, <YW18FIFA>) are transmitted as hash codes. They will be
displayed correctly by receiving stations that have copied the full
callsign without brackets in a previous transmissiion. Otherwise the
receiving software will display <…>. Hash collisions are possible
but should be rare, and extremely rare within a particular QSO.
*/

#ifndef _radioFT8modulator_f32_h
#define _radioFT8modulator_f32_h

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "OpenAudio_ArduinoLibrary.h"
#include <math.h>

#define GFSK_CONST_K 5.336446f // pi*sqrt(2 / log(2))
#define FT8_SYMBOL_BT 2.0f // symbol smoothing filter bandwidth factor (BT)
// #define FT4_SYMBOL_BT 1.0f    No FT4 here

class RadioFT8Modulator_F32 : public AudioStream_F32 {
//GUI: inputs:0, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName: FT8modulator
public:
   // Option of AudioSettings_F32 change to block size or sample rate:
   RadioFT8Modulator_F32(void) :  AudioStream_F32(0, NULL) {
       // Defaults
      }
/*   RadioFT8Modulator_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL) {
      setSampleRate_Hz(settings.sampleRateHz);
      block_size = settings.audio_block_samples;
      }
 */

   void ft8Initialize(void)  {
      /* The Gaussian smoothing "pulse" is computed at setup for the current
       * transmit tone and one before and after.  This optimizes speed.  It is
       * reasonable to compute the time shifted segments at update() time and
       * save about 2*toneLength float storage.  It can be continued further by
       * noting that about half of the values are either 0.0 or 1.0.
       * So we produce values from  only the rising values over the last 37%.
       * The rest can be derived simply enough.
       * toneLength = 7680 audio samples from 0.16 sec at 48 kHz.
       * pulseSegActive is the length for which float32_t is meaningful.
       * pulseSegActive = 5632 and 2816 in length.
       * The subscripts are offset by  toneLength - pulseSegActive
       *                                    15360 - 5632 = 9728 for 96 kHz
       *                                     7680 - 2816 = 4864 for 48 kHz
       */
      for (int i=toneLength-pulseSegActive; i<toneLength; i++)
         {
         // If an odd number of pulse samples:
         // float t = (((float32_t)i)/(float32_t)toneLength) - 1.5f;
         // If an even number of pulse samples:
         float t = ((0.5f + (float32_t)i)/(float32_t)toneLength) - 1.5f;
         float arg1 = GFSK_CONST_K * FT8_SYMBOL_BT * (t + 0.5f);
         float arg2 = GFSK_CONST_K * FT8_SYMBOL_BT * (t - 0.5f);
         pulse[i - toneLength + pulseSegActive] = 0.5f*(erff(arg1) - erff(arg2));
         }
         // Print the pulse segment
         //for (int i=0; i<pulseSegActive; i++)
         //   {
	      //   Serial.print(i);
         //   Serial.print(",");
         //   Serial.println(pulse[i], 8);
         //   }
         // 0,   0.00000003   (Results)
         //     ...
         // 2813,0.49803999
         // 2814,0.49882385
         // 2815,0.49960807

      // Raised cosine ramp to start and end the transmission
      // (key click elimination).  Ranges (0, 0.000000) to (959, 0.999997)
      // for 48 kHz sample rate
      for(int ii=0; ii<cosineRampActive; ii++)
         {
         ramp[ii] = 0.5*(1.0f - cosf(PI*(float32_t)ii/cosineRampActive));
         // Serial.println(ramp[ii], 6);
         }
   }

   // Is transmission active?
   bool FT8TransmitBusy(void)  {
      return ft8Transmit;
      }

   // As viewed from the INO, put ASCII into _data array to be sent.
   // Returns true if successful.
   // *Special Cases*
   // _specialCase==0:
   //    Not special at all.  Converts the message to tones and transmits
   //    the 81 tones in FT8 format.
   // _specialCase = 1:
   //    This allows running of the message-to-tone conversion without
   //    starting the transmission of tones.  Results are in tones[].
   //    To keep things simple, this *cannot* be done when tones are
   //    being transmitted.  See cancelTransmit() below, if needed.
   // _specialCase==2:
   //    This starts the transmission of tones[]
   //    without doing a message-to-tone as part of the function call.
   //    Again, only when not transmitting.  See getTones() below for pointer
   //    to the array that can be filled.
   bool sendData(char* _data, int16_t _i3, int16_t _n3, int16_t _specialCase) {
      if( ft8Transmit ) {  // Busy transmitting
         return false;
         }
      dataFT8 = _data;
      i3 = _i3;        // Todo - These are not implemented
      n3 = _n3;
      if(_specialCase!=2)
         {
         pack77(dataFT8, packedPayload);  // Build the FT8 payload bit array
         genft8(packedPayload, &tones[1]);  // Shift one to allow for 2 key-click tones
         // Add key click suppression tones, with slow 20 mSec transitions
         tones[0] = tones[1];   // Make same freq as [1]
         tones[80] = tones[79];
         tone81SendCount = 0;
         }
      if(_specialCase!=1)
         {
         ft8Transmit = true;    // Start new 81 tone, 12.96 second transmit
         tone81SendCount = 0;
         }
      return true;
      }

   // Sets base frequency for FT-8 modulation transmission.  Tones 0 to 7
   // raise the frequency by 0 to 7 times 6.25 Hz.
   void frequency(float32_t _f0) {
      freqFT8 = _f0;
      phaseIncrement = kp*freqFT8;
      }

   // The amplitude, a, is the peak, as in zero-to-peak.  This produces outputs
   // ranging from -a to +a.
   void amplitude(float32_t a) {
      if (a < 0.0f)  a = 0.0f;
      magnitude = a;
      }

   // Following reflects that there are only 2 supported sample rates.
   // IMPORTANT: This changes constants for FT8 Transmit, only.  It does not
   // change system-wide audio sample rate.  Use AudioSettings_F32.
   void setSampleRate_Hz(const float32_t &fs_Hz) {
      sampleRateHz = fs_Hz;
      if(sampleRateHz>47900.0f && sampleRateHz<48100.0f)
         {
         sampleRateHz = 48000.0f;
         }
      else if(sampleRateHz>95900.0f && sampleRateHz<96100.0f)
         {
         sampleRateHz = 96000.0f;
         }
      else
         {
         Serial.println("Unsupported sample rate, FT-8 will not transmit.");
         }
      kp = 512.0f/sampleRateHz;
      toneLength = (int32_t)(0.5f + 0.16f*sampleRateHz);
      pulseSegActive = (int32_t)(0.5f + 0.3666667f*toneLength);
      phaseIncrement = kp*freqFT8;
      dphi_peak = 512.0f*6.25f/sampleRateHz;
      cosineRampActive = (int32_t)(0.5f + 0.020f*sampleRateHz);
      }

   void cancelTransmit(void)  {
      ft8Transmit = false;
      }

   // Returns pointer to array of 10 bytes holding 77 bit payload
   uint8_t* getPayload(void) {
      return packedPayload;
      }

   // Returns pointer to 81 bytes holding the tones to be transmitted (0, 7)
   uint8_t* getTones(void) {
      return tones;
      }

   // Diagnostic function.  Caution, this may be removed in the future.
   void printFT8TransmitInfo(void)  {
      if(ft8Transmit)
         Serial.println("FT8 in Transmission");
      else
         Serial.println("FT8 Transmit Idle");
      Serial.print("Sample Rate, Hz      = "); Serial.println(sampleRateHz);
      Serial.print("kp, 512 per SR       = "); Serial.println(kp, 8);
      Serial.print("Tone Length, samples = "); Serial.println(toneLength);
      Serial.print("Pulse Segment Active = "); Serial.println(pulseSegActive);
      Serial.print("Cosine Ramp Active   = "); Serial.println(cosineRampActive);
      Serial.print("FT8 Base Frequency   = "); Serial.println(freqFT8);
      Serial.print("FT8 Frequency        = "); Serial.println(dphi/kp);
      Serial.print("Current Tone Number  = "); Serial.println(tone81SendCount);
      Serial.print("Current Tone         = "); Serial.println(tones[tone81SendCount]);
      Serial.print("Sample being Sent    = "); Serial.println(sampleSent);
      Serial.println("");
      }

   virtual void update(void);

private:
   float32_t sampleRateHz = AUDIO_SAMPLE_RATE_EXACT;
   // Only 2 sample rates are supported
   // block_size other than 128 is not supported:
   uint16_t  blockSize = 128;
   int32_t   toneLength = (int32_t)(0.5f + 0.16f*sampleRateHz);
   int32_t   pulseSegActive = (int32_t)(0.5f + 0.3666667f*toneLength);
   int32_t   cosineRampActive = (int32_t)(0.5f + 0.020f*sampleRateHz);
   float32_t freqFT8 = 1000.0f;
   float32_t kp = 512.0f/sampleRateHz;
   float32_t phaseIncrement = kp*freqFT8;
   float32_t currentPhase = 0.0f;
   float32_t dphi = 0.0f;
   float32_t dphi_peak = 512.0f*6.25f/sampleRateHz;
   float32_t magnitude = 1.0f;
   int32_t  tone81SendCount = 0;
   int32_t  audioSampleCount = 0;
   int32_t  sampleSent = 0;
   float32_t ramp[1920];    // Raised cosine ramp 20mSec at 96 kHz sample rate
   float32_t pulse[5632];   // Non zero, non-one part of pulse rising 0.0 to 0.5
   bool ft8Transmit = false;
   char* dataFT8 = NULL;
   uint16_t i3 = 0;
   uint16_t n3 = 0;

   const int ND = 58;      // Data symbols
   const int NS = 21;      // Sync symbols (3 @ Costas 7x7)
   const int NN = 79;   // Total channel symbols (79)
   // Define the LDPC sizes
   const int N = 174;      // Number of bits in the encoded message
   const int K = 91;       // Number of payload bits
   const int M = 83;
   const int K_BYTES = 12;
   // Define CRC parameters
   uint16_t CRC_POLYNOMIAL = 0x2757;  // CRC-14 polynomial without the leading (MSB) 1
   const int CRC_WIDTH = 14;
   uint8_t packedPayload[10];
   uint8_t tones[81];            // 79 + 2 for key click reduction
 //  const int rampSize[4] = {0, 8, 9, 16}; // For none plus the 3 sample rates allowed

/* -----------------------------------------------------------------------
 * The remainder of this was brazenly borrowed from Karlis Goba's
 * https://github.com/kgoba/ft8_lib/blob/master/ft8/
 * and retains his "todo's," etc.  Thanks to the whole chain of authors.
 * Under same MIT license as above with  ** Copyright (c) 2018 Kārlis Goba **
 */

// Costas 7x7 tone pattern
const uint8_t kCostas_map[7] = { 3,1,4,0,6,5,2 };

// Gray code map
const uint8_t kGray_map[8] = { 0,1,3,2,5,6,4,7 };

// Parity generator matrix for (174,91) LDPC code, stored in bitpacked format (MSB first)
const uint8_t kGenerator[83][12] = {
   { 0x83, 0x29, 0xce, 0x11, 0xbf, 0x31, 0xea, 0xf5, 0x09, 0xf2, 0x7f, 0xc0 },
   { 0x76, 0x1c, 0x26, 0x4e, 0x25, 0xc2, 0x59, 0x33, 0x54, 0x93, 0x13, 0x20 },
   { 0xdc, 0x26, 0x59, 0x02, 0xfb, 0x27, 0x7c, 0x64, 0x10, 0xa1, 0xbd, 0xc0 },
   { 0x1b, 0x3f, 0x41, 0x78, 0x58, 0xcd, 0x2d, 0xd3, 0x3e, 0xc7, 0xf6, 0x20 },
   { 0x09, 0xfd, 0xa4, 0xfe, 0xe0, 0x41, 0x95, 0xfd, 0x03, 0x47, 0x83, 0xa0 },
   { 0x07, 0x7c, 0xcc, 0xc1, 0x1b, 0x88, 0x73, 0xed, 0x5c, 0x3d, 0x48, 0xa0 },
   { 0x29, 0xb6, 0x2a, 0xfe, 0x3c, 0xa0, 0x36, 0xf4, 0xfe, 0x1a, 0x9d, 0xa0 },
   { 0x60, 0x54, 0xfa, 0xf5, 0xf3, 0x5d, 0x96, 0xd3, 0xb0, 0xc8, 0xc3, 0xe0 },
   { 0xe2, 0x07, 0x98, 0xe4, 0x31, 0x0e, 0xed, 0x27, 0x88, 0x4a, 0xe9, 0x00 },
   { 0x77, 0x5c, 0x9c, 0x08, 0xe8, 0x0e, 0x26, 0xdd, 0xae, 0x56, 0x31, 0x80 },
   { 0xb0, 0xb8, 0x11, 0x02, 0x8c, 0x2b, 0xf9, 0x97, 0x21, 0x34, 0x87, 0xc0 },
   { 0x18, 0xa0, 0xc9, 0x23, 0x1f, 0xc6, 0x0a, 0xdf, 0x5c, 0x5e, 0xa3, 0x20 },
   { 0x76, 0x47, 0x1e, 0x83, 0x02, 0xa0, 0x72, 0x1e, 0x01, 0xb1, 0x2b, 0x80 },
   { 0xff, 0xbc, 0xcb, 0x80, 0xca, 0x83, 0x41, 0xfa, 0xfb, 0x47, 0xb2, 0xe0 },
   { 0x66, 0xa7, 0x2a, 0x15, 0x8f, 0x93, 0x25, 0xa2, 0xbf, 0x67, 0x17, 0x00 },
   { 0xc4, 0x24, 0x36, 0x89, 0xfe, 0x85, 0xb1, 0xc5, 0x13, 0x63, 0xa1, 0x80 },
   { 0x0d, 0xff, 0x73, 0x94, 0x14, 0xd1, 0xa1, 0xb3, 0x4b, 0x1c, 0x27, 0x00 },
   { 0x15, 0xb4, 0x88, 0x30, 0x63, 0x6c, 0x8b, 0x99, 0x89, 0x49, 0x72, 0xe0 },
   { 0x29, 0xa8, 0x9c, 0x0d, 0x3d, 0xe8, 0x1d, 0x66, 0x54, 0x89, 0xb0, 0xe0 },
   { 0x4f, 0x12, 0x6f, 0x37, 0xfa, 0x51, 0xcb, 0xe6, 0x1b, 0xd6, 0xb9, 0x40 },
   { 0x99, 0xc4, 0x72, 0x39, 0xd0, 0xd9, 0x7d, 0x3c, 0x84, 0xe0, 0x94, 0x00 },
   { 0x19, 0x19, 0xb7, 0x51, 0x19, 0x76, 0x56, 0x21, 0xbb, 0x4f, 0x1e, 0x80 },
   { 0x09, 0xdb, 0x12, 0xd7, 0x31, 0xfa, 0xee, 0x0b, 0x86, 0xdf, 0x6b, 0x80 },
   { 0x48, 0x8f, 0xc3, 0x3d, 0xf4, 0x3f, 0xbd, 0xee, 0xa4, 0xea, 0xfb, 0x40 },
   { 0x82, 0x74, 0x23, 0xee, 0x40, 0xb6, 0x75, 0xf7, 0x56, 0xeb, 0x5f, 0xe0 },
   { 0xab, 0xe1, 0x97, 0xc4, 0x84, 0xcb, 0x74, 0x75, 0x71, 0x44, 0xa9, 0xa0 },
   { 0x2b, 0x50, 0x0e, 0x4b, 0xc0, 0xec, 0x5a, 0x6d, 0x2b, 0xdb, 0xdd, 0x00 },
   { 0xc4, 0x74, 0xaa, 0x53, 0xd7, 0x02, 0x18, 0x76, 0x16, 0x69, 0x36, 0x00 },
   { 0x8e, 0xba, 0x1a, 0x13, 0xdb, 0x33, 0x90, 0xbd, 0x67, 0x18, 0xce, 0xc0 },
   { 0x75, 0x38, 0x44, 0x67, 0x3a, 0x27, 0x78, 0x2c, 0xc4, 0x20, 0x12, 0xe0 },
   { 0x06, 0xff, 0x83, 0xa1, 0x45, 0xc3, 0x70, 0x35, 0xa5, 0xc1, 0x26, 0x80 },
   { 0x3b, 0x37, 0x41, 0x78, 0x58, 0xcc, 0x2d, 0xd3, 0x3e, 0xc3, 0xf6, 0x20 },
   { 0x9a, 0x4a, 0x5a, 0x28, 0xee, 0x17, 0xca, 0x9c, 0x32, 0x48, 0x42, 0xc0 },
   { 0xbc, 0x29, 0xf4, 0x65, 0x30, 0x9c, 0x97, 0x7e, 0x89, 0x61, 0x0a, 0x40 },
   { 0x26, 0x63, 0xae, 0x6d, 0xdf, 0x8b, 0x5c, 0xe2, 0xbb, 0x29, 0x48, 0x80 },
   { 0x46, 0xf2, 0x31, 0xef, 0xe4, 0x57, 0x03, 0x4c, 0x18, 0x14, 0x41, 0x80 },
   { 0x3f, 0xb2, 0xce, 0x85, 0xab, 0xe9, 0xb0, 0xc7, 0x2e, 0x06, 0xfb, 0xe0 },
   { 0xde, 0x87, 0x48, 0x1f, 0x28, 0x2c, 0x15, 0x39, 0x71, 0xa0, 0xa2, 0xe0 },
   { 0xfc, 0xd7, 0xcc, 0xf2, 0x3c, 0x69, 0xfa, 0x99, 0xbb, 0xa1, 0x41, 0x20 },
   { 0xf0, 0x26, 0x14, 0x47, 0xe9, 0x49, 0x0c, 0xa8, 0xe4, 0x74, 0xce, 0xc0 },
   { 0x44, 0x10, 0x11, 0x58, 0x18, 0x19, 0x6f, 0x95, 0xcd, 0xd7, 0x01, 0x20 },
   { 0x08, 0x8f, 0xc3, 0x1d, 0xf4, 0xbf, 0xbd, 0xe2, 0xa4, 0xea, 0xfb, 0x40 },
   { 0xb8, 0xfe, 0xf1, 0xb6, 0x30, 0x77, 0x29, 0xfb, 0x0a, 0x07, 0x8c, 0x00 },
   { 0x5a, 0xfe, 0xa7, 0xac, 0xcc, 0xb7, 0x7b, 0xbc, 0x9d, 0x99, 0xa9, 0x00 },
   { 0x49, 0xa7, 0x01, 0x6a, 0xc6, 0x53, 0xf6, 0x5e, 0xcd, 0xc9, 0x07, 0x60 },
   { 0x19, 0x44, 0xd0, 0x85, 0xbe, 0x4e, 0x7d, 0xa8, 0xd6, 0xcc, 0x7d, 0x00 },
   { 0x25, 0x1f, 0x62, 0xad, 0xc4, 0x03, 0x2f, 0x0e, 0xe7, 0x14, 0x00, 0x20 },
   { 0x56, 0x47, 0x1f, 0x87, 0x02, 0xa0, 0x72, 0x1e, 0x00, 0xb1, 0x2b, 0x80 },
   { 0x2b, 0x8e, 0x49, 0x23, 0xf2, 0xdd, 0x51, 0xe2, 0xd5, 0x37, 0xfa, 0x00 },
   { 0x6b, 0x55, 0x0a, 0x40, 0xa6, 0x6f, 0x47, 0x55, 0xde, 0x95, 0xc2, 0x60 },
   { 0xa1, 0x8a, 0xd2, 0x8d, 0x4e, 0x27, 0xfe, 0x92, 0xa4, 0xf6, 0xc8, 0x40 },
   { 0x10, 0xc2, 0xe5, 0x86, 0x38, 0x8c, 0xb8, 0x2a, 0x3d, 0x80, 0x75, 0x80 },
   { 0xef, 0x34, 0xa4, 0x18, 0x17, 0xee, 0x02, 0x13, 0x3d, 0xb2, 0xeb, 0x00 },
   { 0x7e, 0x9c, 0x0c, 0x54, 0x32, 0x5a, 0x9c, 0x15, 0x83, 0x6e, 0x00, 0x00 },
   { 0x36, 0x93, 0xe5, 0x72, 0xd1, 0xfd, 0xe4, 0xcd, 0xf0, 0x79, 0xe8, 0x60 },
   { 0xbf, 0xb2, 0xce, 0xc5, 0xab, 0xe1, 0xb0, 0xc7, 0x2e, 0x07, 0xfb, 0xe0 },
   { 0x7e, 0xe1, 0x82, 0x30, 0xc5, 0x83, 0xcc, 0xcc, 0x57, 0xd4, 0xb0, 0x80 },
   { 0xa0, 0x66, 0xcb, 0x2f, 0xed, 0xaf, 0xc9, 0xf5, 0x26, 0x64, 0x12, 0x60 },
   { 0xbb, 0x23, 0x72, 0x5a, 0xbc, 0x47, 0xcc, 0x5f, 0x4c, 0xc4, 0xcd, 0x20 },
   { 0xde, 0xd9, 0xdb, 0xa3, 0xbe, 0xe4, 0x0c, 0x59, 0xb5, 0x60, 0x9b, 0x40 },
   { 0xd9, 0xa7, 0x01, 0x6a, 0xc6, 0x53, 0xe6, 0xde, 0xcd, 0xc9, 0x03, 0x60 },
   { 0x9a, 0xd4, 0x6a, 0xed, 0x5f, 0x70, 0x7f, 0x28, 0x0a, 0xb5, 0xfc, 0x40 },
   { 0xe5, 0x92, 0x1c, 0x77, 0x82, 0x25, 0x87, 0x31, 0x6d, 0x7d, 0x3c, 0x20 },
   { 0x4f, 0x14, 0xda, 0x82, 0x42, 0xa8, 0xb8, 0x6d, 0xca, 0x73, 0x35, 0x20 },
   { 0x8b, 0x8b, 0x50, 0x7a, 0xd4, 0x67, 0xd4, 0x44, 0x1d, 0xf7, 0x70, 0xe0 },
   { 0x22, 0x83, 0x1c, 0x9c, 0xf1, 0x16, 0x94, 0x67, 0xad, 0x04, 0xb6, 0x80 },
   { 0x21, 0x3b, 0x83, 0x8f, 0xe2, 0xae, 0x54, 0xc3, 0x8e, 0xe7, 0x18, 0x00 },
   { 0x5d, 0x92, 0x6b, 0x6d, 0xd7, 0x1f, 0x08, 0x51, 0x81, 0xa4, 0xe1, 0x20 },
   { 0x66, 0xab, 0x79, 0xd4, 0xb2, 0x9e, 0xe6, 0xe6, 0x95, 0x09, 0xe5, 0x60 },
   { 0x95, 0x81, 0x48, 0x68, 0x2d, 0x74, 0x8a, 0x38, 0xdd, 0x68, 0xba, 0xa0 },
   { 0xb8, 0xce, 0x02, 0x0c, 0xf0, 0x69, 0xc3, 0x2a, 0x72, 0x3a, 0xb1, 0x40 },
   { 0xf4, 0x33, 0x1d, 0x6d, 0x46, 0x16, 0x07, 0xe9, 0x57, 0x52, 0x74, 0x60 },
   { 0x6d, 0xa2, 0x3b, 0xa4, 0x24, 0xb9, 0x59, 0x61, 0x33, 0xcf, 0x9c, 0x80 },
   { 0xa6, 0x36, 0xbc, 0xbc, 0x7b, 0x30, 0xc5, 0xfb, 0xea, 0xe6, 0x7f, 0xe0 },
   { 0x5c, 0xb0, 0xd8, 0x6a, 0x07, 0xdf, 0x65, 0x4a, 0x90, 0x89, 0xa2, 0x00 },
   { 0xf1, 0x1f, 0x10, 0x68, 0x48, 0x78, 0x0f, 0xc9, 0xec, 0xdd, 0x80, 0xa0 },
   { 0x1f, 0xbb, 0x53, 0x64, 0xfb, 0x8d, 0x2c, 0x9d, 0x73, 0x0d, 0x5b, 0xa0 },
   { 0xfc, 0xb8, 0x6b, 0xc7, 0x0a, 0x50, 0xc9, 0xd0, 0x2a, 0x5d, 0x03, 0x40 },
   { 0xa5, 0x34, 0x43, 0x30, 0x29, 0xea, 0xc1, 0x5f, 0x32, 0x2e, 0x34, 0xc0 },
   { 0xc9, 0x89, 0xd9, 0xc7, 0xc3, 0xd3, 0xb8, 0xc5, 0x5d, 0x75, 0x13, 0x00 },
   { 0x7b, 0xb3, 0x8b, 0x2f, 0x01, 0x86, 0xd4, 0x66, 0x43, 0xae, 0x96, 0x20 },
   { 0x26, 0x44, 0xeb, 0xad, 0xeb, 0x44, 0xb9, 0x46, 0x7d, 0x1f, 0x42, 0xc0 },
   { 0x60, 0x8c, 0xc8, 0x57, 0x59, 0x4b, 0xfb, 0xb5, 0x5d, 0x69, 0x60, 0x00 }
};

// TODO: This is wasteful, should figure out something more elegant
   const char A0s[43] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?";
   const char A1s[38] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
   const char A2s[37] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
   const char A3s[11] = "0123456789";
   const char A4s[28] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";

   // Pack a special token, a 22-bit hash code, or a valid base call
   // into a 28-bit integer.
   int32_t pack28(const char *callsign) {
      int32_t NTOKENS = 2063592L;
      int32_t MAX22   = 4194304L;

      // Check for special tokens first
      if (starts_with(callsign, "DE "))   return 0;
      if (starts_with(callsign, "QRZ "))  return 1;
      if (starts_with(callsign, "CQ "))   return 2;

      if (starts_with(callsign, "CQ_"))
         {
         //int nnum = 0, nlet = 0;
         // TODO:
         // if(nnum.eq.3 .and. nlet.eq.0) then n28=3+nqsy
         // if(nlet.ge.1 .and. nlet.le.4 .and. nnum.eq.0) then n28=3+1000+m
         }

      // TODO: Check for <...> callsign
      // if(text(1:1).eq.'<')then
      //   call save_hash_call(text,n10,n12,n22)   !Save callsign in hash table
      //   n28=NTOKENS + n22
      char c6[6] = {' ', ' ', ' ', ' ', ' ', ' '};

      int length = 0; // strlen(callsign);  // We will need it later
      while (callsign[length] != ' ' && callsign[length] != 0)
         {
         length++;
         }
      // Copy callsign to 6 character buffer
      if (starts_with(callsign, "3DA0") && length <= 7)
         {
         // Work-around for Swaziland prefix: 3DA0XYZ -> 3D0XYZ
         memcpy(c6, "3D0", 3);
         memcpy(c6 + 3, callsign + 4, length - 4);
         }
      else if (starts_with(callsign, "3X") && is_letter(callsign[2]) && length <= 7)
         {
         // Work-around for Guinea prefixes: 3XA0XYZ -> QA0XYZ
         memcpy(c6, "Q", 1);
         memcpy(c6 + 1, callsign + 2, length - 2);
         }
      else
         {
         if (is_digit(callsign[2]) && length <= 6)
            {
            // AB0XYZ
            memcpy(c6, callsign, length);
            }
         else if (is_digit(callsign[1]) && length <= 5)
            {
            // A0XYZ -> " A0XYZ"
            memcpy(c6 + 1, callsign, length);
            }
         }
      // Check for standard callsign
      int i0, i1, i2, i3, i4, i5;
      if((i0 = char_index(A1s, c6[0])) >= 0 && (i1 = char_index(A2s, c6[1])) >= 0 &&
         (i2 = char_index(A3s, c6[2])) >= 0 && (i3 = char_index(A4s, c6[3])) >= 0 &&
         (i4 = char_index(A4s, c6[4])) >= 0 && (i5 = char_index(A4s, c6[5])) >= 0)
         {
         //printf("Pack28: idx=[%d, %d, %d, %d, %d, %d]\n", i0, i1, i2, i3, i4, i5);
         // This is a standard callsign
         int32_t n28 = i0;
         n28 = n28 * 36 + i1;
         n28 = n28 * 10 + i2;
         n28 = n28 * 27 + i3;
         n28 = n28 * 27 + i4;
         n28 = n28 * 27 + i5;
         //printf("Pack28: n28=%d (%04xh)\n", n28, n28);
         return NTOKENS + MAX22 + n28;
         }
      //char text[13];
      //if (length > 13) return -1;

      // TODO:
      // Treat this as a nonstandard callsign: compute its 22-bit hash
      // call save_hash_call(text,n10,n12,n22)   !Save callsign in hash table
      // n28=NTOKENS + n22
      // n28=iand(n28,ishft(1,28)-1)
      return -1;
   }      // End pack28()

// Check if a string could be a valid standard callsign or a valid
// compound callsign.
// Return base call "bc" and a logical "cok" indicator.
bool chkcall(const char *call, char *bc) {
   int length = strlen(call);   // n1=len_trim(w)
   if (length > 11) return false;
   if (0 != strchr(call, '.')) return false;
   if (0 != strchr(call, '+')) return false;
   if (0 != strchr(call, '-')) return false;
   if (0 != strchr(call, '?')) return false;
   if (length > 6 && 0 != strchr(call, '/')) return false;

   // TODO: implement suffix parsing (or rework?)
   //bc=w(1:6)
   //i0=char_index(w,'/')
   //if(max(i0-1,n1-i0).gt.6) go to 100      !Base call must be < 7 characters
   //if(i0.ge.2 .and. i0.le.n1-1) then       !Extract base call from compound call
   //   if(i0-1.le.n1-i0) bc=w(i0+1:n1)//'   '
   //   if(i0-1.gt.n1-i0) bc=w(1:i0-1)//'   '

    return true;
   }

uint16_t packgrid(const char *grid4) {
    //constexpr uint16_t MAXGRID4 = 32400;
    uint16_t MAXGRID4 = 32400;

    if (grid4 == 0) {
        // Two callsigns only, no report/grid
        return MAXGRID4 + 1;
    }

    // Take care of special cases
    if (equals(grid4, "RRR")) return MAXGRID4 + 2;
    if (equals(grid4, "RR73")) return MAXGRID4 + 3;
    if (equals(grid4, "73")) return MAXGRID4 + 4;

    // Check for standard 4 letter grid
    if (in_range(grid4[0], 'A', 'R') &&
        in_range(grid4[1], 'A', 'R') &&
        is_digit(grid4[2]) && is_digit(grid4[3]))
    {
        //if (w(3).eq.'R ') ir=1
        uint16_t igrid4 = (grid4[0] - 'A');
        igrid4 = igrid4 * 18 + (grid4[1] - 'A');
        igrid4 = igrid4 * 10 + (grid4[2] - '0');
        igrid4 = igrid4 * 10 + (grid4[3] - '0');
        return igrid4;
    }

    // Parse report: +dd / -dd / R+dd / R-dd
    // TODO: check the range of dd
    if (grid4[0] == 'R') {
        int dd = dd_to_int(grid4 + 1, 3);
        uint16_t irpt = 35 + dd;
        return (MAXGRID4 + irpt) | 0x8000;  // ir = 1
    }
    else {
        int dd = dd_to_int(grid4, 3);
        uint16_t irpt = 35 + dd;
        return (MAXGRID4 + irpt);           // ir = 0
    }

    return MAXGRID4 + 1;
   }

// Pack Type 1 (Standard 77-bit message) and Type 2 (ditto, with a "/P" call)
int pack77_1(const char *msg, uint8_t *b77) {
    // Locate the first delimiter
    const char *s1 = strchr(msg, ' ');
    if (s1 == 0) return -1;

    const char *call1 = msg;        // 1st call
    const char *call2 = s1 + 1;     // 2nd call

    int32_t n28a = pack28(call1);
    int32_t n28b = pack28(call2);

    if (n28a < 0 || n28b < 0) return -1;

    uint16_t igrid4;

    // Locate the second delimiter
    const char *s2 = strchr(s1 + 1, ' ');
    if (s2 != 0) {
        igrid4 = packgrid(s2 + 1);
    }
    else {
        // Two callsigns, no grid/report
        igrid4 = packgrid(0);
    }

    uint8_t i3 = 1; // No suffix or /R

    // TODO: check for suffixes
    // if(char_index(w(1),'/P').ge.4 .or. char_index(w(2),'/P').ge.4) i3=2  !Type 2, with "/P"
    // if(char_index(w(1),'/P').ge.4 .or. char_index(w(1),'/R').ge.4) ipa=1
    // if(char_index(w(2),'/P').ge.4 .or. char_index(w(2),'/R').ge.4) ipb=1

    // Shift in ipa and ipb bits into n28a and n28b
    n28a <<= 1; // ipa = 0
    n28b <<= 1; // ipb = 0

    // Pack into (28 + 1) + (28 + 1) + (1 + 15) + 3 bits
    // write(c77,1000) n28a,ipa,n28b,ipb,ir,igrid4,i3
    // 1000 format(2(b28.28,b1),b1,b15.15,b3.3)

    b77[0] = (n28a >> 21);
    b77[1] = (n28a >> 13);
    b77[2] = (n28a >> 5);
    b77[3] = (uint8_t)(n28a << 3) | (uint8_t)(n28b >> 26);
    b77[4] = (n28b >> 18);
    b77[5] = (n28b >> 10);
    b77[6] = (n28b >> 2);
    b77[7] = (uint8_t)(n28b << 6) | (uint8_t)(igrid4 >> 10);
    b77[8] = (igrid4 >> 2);
    b77[9] = (uint8_t)(igrid4 << 6) | (uint8_t)(i3 << 3);

    return 0;
   }

void packtext77(const char *text, uint8_t *b77) {
    int length = strlen(text);

    // Skip leading and trailing spaces
    while (*text == ' ' && *text != 0) {
        ++text;
        --length;
    }
    while (length > 0 && text[length - 1] == ' ') {
        --length;
    }

    // Clear the first 72 bits representing a long number
    for (int i = 0; i < 9; ++i) {
        b77[i] = 0;
    }

    // Now express the text as base-42 number stored
    // in the first 72 bits of b77
    for (int j = 0; j < 13; ++j) {
        // Multiply the long integer in b77 by 42
        uint16_t x = 0;
        for (int i = 8; i >= 0; --i) {
            x += b77[i] * (uint16_t)42;
            b77[i] = (x & 0xFF);
            x >>= 8;
        }

        // Get the index of the current char
        if (j < length) {
            int q = char_index(A0s, text[j]);
            x = (q > 0) ? q : 0;
        }
        else {
            x = 0;
        }
        // Here we double each added number in order to have the result multiplied
        // by two as well, so that it's a 71 bit number left-aligned in 72 bits (9 bytes)
        x <<= 1;

        // Now add the number to our long number
        for (int i = 8; i >= 0; --i) {
            if (x == 0) break;
            x += b77[i];
            b77[i] = (x & 0xFF);
            x >>= 8;
        }
    }

    // Set n3=0 (bits 71..73) and i3=0 (bits 74..76)
    b77[8] &= 0xFE;
    b77[9] &= 0x00;
   }

// pack77()
// [IN] msg - 10 byte array consisting of 77 bit payload (MSB first)
// [OUT] payload - 10 byte array consisting of 77 bit payload (MSB first)
int pack77(const char *msg, uint8_t *c77) {
    // Check Type 1 (Standard 77-bit message) or Type 2, with optional "/P"
    if (0 == pack77_1(msg, c77))
        {
        return 0;
        }

    // TODO:
    // Check 0.5 (telemetry)
    // i3=0 n3=5 write(c77,1006) ntel,n3,i3 1006 format(b23.23,2b24.24,2b3.3)

    // Check Type 4 (One nonstandard call and one hashed call)
    // pack77_4(nwords,w,i3,n3,c77)

    // Default to free text
    // i3=0 n3=0
    packtext77(msg, c77);
    return 0;
   }

bool starts_with(const char *string, const char *prefix) {
    return 0 == memcmp(string, prefix, strlen(prefix));
   }

char to_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
   }

bool is_digit(char c) {
    return (c >= '0') && (c <= '9');
   }

bool is_letter(char c) {
    return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'));
   }

bool is_space(char c) {
    return (c == ' ');
   }

bool in_range(char c, char min, char max) {
    return (c >= min) && (c <= max);
   }

bool equals(const char *string1, const char *string2) {
    return 0 == strcmp(string1, string2);
   }

int char_index(const char *string, char c) {
    for (int i = 0; *string; ++i, ++string) {
        if (c == *string) {
            return i;
        }
    }
    return -1;  // Not found
   }

// Parse a 2 digit integer from string
int dd_to_int(const char *str, int length) {
    int result = 0;
    bool negative;
    int i;
    if (str[0] == '-') {
        negative = true;
        i = 1;                          // Consume the - sign
    }
    else {
        negative = false;
        i = (str[0] == '+') ? 1 : 0;    // Consume a + sign if found
    }
    while (i < length) {
        if (str[i] == 0) break;
        if (!is_digit(str[i])) break;
        result *= 10;
        result += (str[i] - '0');
        ++i;
    }
    return negative ? -result : result;
}

// Convert a 2 digit integer to string
void int_to_dd(char *str, int value, int width, bool full_sign) {
    if (value < 0) {
        *str = '-';
        ++str;
        value = -value;
    }
    else if (full_sign) {
        *str = '+';
        ++str;
    }
    int divisor = 1;
    for (int i = 0; i < width - 1; ++i) {
        divisor *= 10;
    }
    while (divisor >= 1) {
        int digit = value / divisor;
        *str = '0' + digit;
        ++str;
        value -= digit * divisor;
        divisor /= 10;
    }
    *str = 0;   // Add zero terminator
   }

// Returns 1 if an odd number of bits are set in x, zero otherwise
uint8_t parity8(uint8_t x) {
    x ^= x >> 4;    // a b c d ae bf cg dh
    x ^= x >> 2;    // a b ac bd cae dbf aecg bfdh
    x ^= x >> 1;    // a ab bac acbd bdcae caedbf aecgbfdh
    return (x) & 1;
   }

// Encode a 91-bit message and return a 174-bit codeword.
// The generator matrix has dimensions (87,87).
// The code is a (174,91) regular ldpc code with column weight 3.
// The code was generated using the PEG algorithm.
// Arguments:
// [IN] message   - array of 91 bits stored as 12 bytes (MSB first)
// [OUT] codeword - array of 174 bits stored as 22 bytes (MSB first)
void encode174(const uint8_t *message, uint8_t *codeword) {
    // Here we don't generate the generator bit matrix as in WSJT-X implementation
    // Instead we access the generator bits straight from the binary representation in kGenerator

    // For reference:
    // codeword(1:K)=message
    // codeword(K+1:N)=pchecks

    // printf("Encode ");
    // for (int i = 0; i < K_BYTES; ++i) {
    //     printf("%02x ", message[i]);
    // }
    // printf("\n");

    // Fill the codeword with message and zeros, as we will only update binary ones later
    for (int j = 0; j < (7 + N) / 8; ++j) {
        codeword[j] = (j < K_BYTES) ? message[j] : 0;
    }

    uint8_t col_mask = (0x80 >> (K % 8));   // bitmask of current byte
    uint8_t col_idx = K_BYTES - 1;          // index into byte array

    // Compute the first part of itmp (1:M) and store the result in codeword
    for (int i = 0; i < M; ++i) { // do i=1,M
        // Fast implementation of bitwise multiplication and parity checking
        // Normally nsum would contain the result of dot product between message and kGenerator[i],
        // but we only compute the sum modulo 2.
        uint8_t nsum = 0;
        for (int j = 0; j < K_BYTES; ++j) {
            uint8_t bits = message[j] & kGenerator[i][j];    // bitwise AND (bitwise multiplication)
            nsum ^= parity8(bits);                  // bitwise XOR (addition modulo 2)
        }
        // Check if we need to set a bit in codeword
        if (nsum % 2) { // pchecks(i)=mod(nsum,2)
            codeword[col_idx] |= col_mask;
        }

        col_mask >>= 1;
        if (col_mask == 0) {
            col_mask = 0x80;
            ++col_idx;
        }
    }

    // printf("Result ");
    // for (int i = 0; i < (N + 7) / 8; ++i) {
    //     printf("%02x ", codeword[i]);
    // }
    // printf("\n");
   }

// Compute 14-bit CRC for a sequence of given number of bits
// [IN] message  - byte sequence (MSB first)
// [IN] num_bits - number of bits in the sequence
uint16_t crc(uint8_t *message, int num_bits) {
    // Adapted from https://barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code
    //constexpr uint16_t  TOPBIT = (1 << (CRC_WIDTH - 1));
    uint16_t  TOPBIT = (1 << (CRC_WIDTH - 1));
    // printf("CRC, %d bits: ", num_bits);
    // for (int i = 0; i < (num_bits + 7) / 8; ++i) {
    //     printf("%02x ", message[i]);
    // }
    // printf("\n");

    uint16_t remainder = 0;
    int idx_byte = 0;

    // Perform modulo-2 division, a bit at a time.
    for (int idx_bit = 0; idx_bit < num_bits; ++idx_bit) {
        if (idx_bit % 8 == 0) {
            // Bring the next byte into the remainder.
            remainder ^= (message[idx_byte] << (CRC_WIDTH - 8));
            ++idx_byte;
        }

        // Try to divide the current data bit.
        if (remainder & TOPBIT) {
            remainder = (remainder << 1) ^ CRC_POLYNOMIAL;
        }
        else {
            remainder = (remainder << 1);
        }
    }
    // printf("CRC = %04xh\n", remainder & ((1 << CRC_WIDTH) - 1));
    return remainder & ((1 << CRC_WIDTH) - 1);
   }

// Generate FT8 tone sequence from payload data
// [IN] payload - 10 byte array consisting of 77 bit payload (MSB first)
// [OUT] itone  - array of NN (79) bytes to store the generated tones (encoded as 0..7)
// This generates 79 tones by historical convention.  Right after calling
// the two key-click tones are added to make 81 total
void genft8(const uint8_t *payload, uint8_t *itone) {
    uint8_t a91[12];    // Store 77 bits of payload + 14 bits CRC

    // Copy 77 bits of payload data
    for (int i = 0; i < 10; i++)
        a91[i] = payload[i];

    // Clear 3 bits after the payload to make 80 bits
    a91[9] &= 0xF8;
    a91[10] = 0;
    a91[11] = 0;

    // Calculate CRC of 12 bytes = 96 bits, see WSJT-X code
    uint16_t checksum = crc(a91, 96 - 14);

    // Store the CRC at the end of 77 bit message
    a91[9] |= (uint8_t)(checksum >> 11);
    a91[10] = (uint8_t)(checksum >> 3);
    a91[11] = (uint8_t)(checksum << 5);

    // a91 contains 77 bits of payload + 14 bits of CRC
    uint8_t codeword[22];
    encode174(a91, codeword);  // Add in 174-91 = 83 bits for FEC

    // itone[] array structure: S7 D29 S7 D29 S7  (79 symbols total)
    // (split 58 codeword symbols into 2 groups of 29)
    for (int i = 0; i < 7; ++i) {
        itone[i]      = kCostas_map[i];
        itone[36 + i] = kCostas_map[i];
        itone[72 + i] = kCostas_map[i];
    }

    int k = 7;          // Skip over the first set of Costas symbols

    uint8_t mask = 0x80;
    int i_byte = 0;
    for (int j=0; j<ND; ++j) {
        if (j == 29) {
            k += 7;     // Skip over the second set of Costas symbols
        }

        // Extract 3 bits from codeword at i-th position
        uint8_t bits3 = 0;

        if (codeword[i_byte] & mask) bits3 |= 4;
        if (0 == (mask >>= 1)) { mask = 0x80; i_byte++; }
        if (codeword[i_byte] & mask) bits3 |= 2;
        if (0 == (mask >>= 1)) { mask = 0x80; i_byte++; }
        if (codeword[i_byte] & mask) bits3 |= 1;
        if (0 == (mask >>= 1)) { mask = 0x80; i_byte++; }

        itone[k] = kGray_map[bits3];
        ++k;
    }
    // Done, now have 79 8-tone symbols decribed by itone[]
    // Raised cosine begin and end is included by first and last Costas tone.
   }

};
#endif
