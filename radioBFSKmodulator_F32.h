/*
 * radioBFSKmodulator_F32.h
 *
 * BFSK Modulator including control line low-pass filtering
 * By Bob Larkin  W7PUA   17 March 2022
 *
 * Thanks to PJRC and Pau Stoffregen for the Teensy processor, Teensyduino
 * and Teensy Audio.  Thanks to Chip Audette for the F32 extension
 * work.  Alll of that makes this possible.  Bob
 *
 * Copyright (c) 2020 Bob Larkin
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
Notes:
Modem type: V.23 forward channel
Data rates: 600-/1200-bps half duplex
Symbol rate: 600/1200 baud (1 bit/symbol)
Sample rate: 9.6 kHz
Mean frequency: Mode 1 (600 bps) 1500 Hz 1 = 1300 Hz 0 = 1700 Hz
Mode 2 (1200 bps) 1700 Hz 1 = 1300 Hz 0 = 2100 Hz
Frequency deviation: Mode 1 ±200 Hz
Mode 2 ±400 Hz
Tx power spectrum: As per V.2 not exceeding -13 dBm
Receiver: FSK receiver structure based on quadrature baseband time-delay
multiply
Rx performance: 600 bps Flat channel: 11-dB SNR for 1e-5 block error rate
(1000 bits/block)
1200 bps Flat channel: 16.5-dB SNR for 1e-5 block error rate
(1000 bits/block)

Modem type: V.23 backward channel
Data rate: 75-bps half duplex
Symbol rate: 75 baud (1 bit/symbol)
Sample rate: 9.6 kHz
Mean frequency: 420 Hz 1 = 390 Hz 0 = 450 Hz
Frequency deviation: ±30 Hz
*/
/*
 * Binary Frequency Shift Keying modulator
 * Very general as this can be used to send 1 to 32 bit words at any
 * rate and frequencies only constrained by fs/2 limitations.  The bit
 * meanings are just 0 or 1.  So, start bits, parity and ending bits
 * are all just part of the transmitted data.   For 8N1 select
 * 10 bits here and construct a 0 start bit and a 1 ending bit.
 * For instance: If ch8 is an 8-bit character, we transmit
 * 0X200 | (ch8 << 1).
 *
 * After a bit is determined, there is an optional low-pass FIR filter to limit
 * the transmit bandwidth.  This produces a non-binary frequency that
 * is set by a phase increment.
 *
 * The update() uses about 13 microseconds for 128 points with no LPF
 * on the control signal.  The LPF adds roughly 3 icroseconds per
 * FIR coefficient being used.
 *
 * 2 April 2023 -Corrected to handle outputs from full buffer.  RSL
 *               Added    int16_t getBufferSpace().  RSL
 */

/* Here is an Octave/Matlab .m file to design the Gaussian LP FIR filter
% FIR Gaussian Pulse-Shaping Filter Design  -  Based roughly on
% https://www.mathworks.com/help/signal/ug/fir-gaussian-pulse-shaping-filter-design.html
%
symbolRate = 1200.0
Ts   = 1.0/symbolRate; % Symbol time (sec)
span = 4;    % Filter span in symbols
sampleRate = 48000.0
overSampleFactor = sampleRate/symbolRate    % For printing info
% nFIR is constrained. Check smallest coefficient and if too big, adjust span
nFIR = span*sampleRate/symbolRate;
if rem(nFIR, 2) == 0
   nFIR = nFIR + 1;
endif
nFIR
%
% Important BT parameter:
BT = 0.4
a = Ts*sqrt(0.5*log(2))/BT;
B = sqrt(log(2)/2)./(a);
B3dB = B       % For printing info
t = linspace(-span*Ts/2, span*Ts/2, nFIR)';
cf32f = zeros(nFIR);         % FIR coefficients
cf32f = sqrt(pi)/a*exp(-(pi*t/a).^2);
cf32f = (cf32f./sum(cf32f))  % Column vector, normalized for DC gain of 1
*/

/* Gaussian FSK LPF Filter  A Sample filter per AIS radios.
 * Paste from here into INO program, or design a different LPF.
 * This filter corresponds to:
 *   symbolRate =  9600
 *   sampleRate =  48000
 *   overSampleFactor = 5
 *   nFIR =  21
 *   BT =  0.40000
 *   B3dB =  3840.0
 *   Atten at 9600 Hz = 18.8 dB
float32_t gaussFIR9600_48K[21] = {
0.0000000029f,
0.0000000934f,
0.0000020700f,
0.0000318610f,
0.0003406053f,
0.0025289299f,
0.0130411453f,
0.0467076746f,
0.1161861408f,
0.2007307811f,
0.2408613913f,
0.2007307811f,
0.1161861408f,
0.0467076746f,
0.0130411453f,
0.0025289299f,
0.0003406053f,
0.0000318610f,
0.0000020700f,
0.0000000934f,
0.0000000029f};  */

#ifndef _radioBFSKmodulator_f32_h
#define _radioBFSKmodulator_f32_h

#include "AudioStream_F32.h"
#include "arm_math.h"
#include "mathDSP_F32.h"
#include <math.h>

class RadioBFSKModulator_F32 : public AudioStream_F32 {
//GUI: inputs:0, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName: BFSKmodulator
public:
   // Option of AudioSettings_F32 change to block size or sample rate:
   RadioBFSKModulator_F32(void) :  AudioStream_F32(0, NULL) {
       // Defaults
      }
   RadioBFSKModulator_F32(const AudioSettings_F32 &settings) : AudioStream_F32(0, NULL) {
      setSampleRate_Hz(settings.sample_rate_Hz);
      block_size = settings.audio_block_samples;
      }

#define DATA_BUFFER_EMPTY  0
#define DATA_BUFFER_PART   1
#define DATA_BUFFER_FULL   2

   // As viewed from the INO, does the bufffer have space?
   bool bufferHasSpace(void)  {
      checkBuffer();
      if(stateBuffer == DATA_BUFFER_FULL)
         return false;
      else
         return true;
      }

   int16_t getBufferSpace(void)  {
      return 64-checkBuffer();
      }

   // As viewed from the INO, put a data word into the buffer to send.
   // Returns true if successful.
   bool sendData(uint32_t data) {
      checkBuffer();
      if(stateBuffer == DATA_BUFFER_FULL)
         return false;
      else
         {
         indexIn++;
         dataBuffer[indexIn & indexMask] = data;
         checkBuffer();
         return true;
         }
      }

   void bufferClear(void)  {
      indexIn = 0LL;
      indexOut = 0LL;
      }

   // Sets all parameters for BFSK modulation
   // Returns number of audio samplesPerDataBit
   uint32_t setBFSK(float32_t _bitRate, uint16_t _numBits, float32_t _f0, float32_t _f1) {
      bitRate = _bitRate;
      numBits = _numBits;
      // Transmission rates are quantized by sample rate. On this transmit side,
      // it seems best to error on the side of sending too fast.  Thus ceilf()
      samplesPerDataBit = (uint16_t)(ceilf((sample_rate_Hz/bitRate) - 0.0000001));

      freq[0] = _f0;
      freq[1] = _f1;
      phaseIncrement[0] = kp*freq[0];
      phaseIncrement[1] = kp*freq[1];
      deltaPhase = kp*(freq[1] - freq[0]);
      return samplesPerDataBit;
      }

   // The amplitude, a, is the peak, as in zero-to-peak.  This produces outputs
   // ranging from -a to +a.
   void amplitude(float32_t a) {
      if (a < 0.0f)  a = 0.0f;
      magnitude = a;
      }

   // Low pass filter on frequency control line.  Set to NULL to omitfilter.
   void setLPF(float32_t* _FIRdata, float32_t* _FIRcoeff, uint16_t _numCoeffs) {
      FIRdata = _FIRdata;
       if(_FIRcoeff == NULL || _numCoeffs == 0)
          {
         FIRcoeff = NULL;
         numCoeffs = 0;
         return;
         }
      FIRcoeff = _FIRcoeff;
      numCoeffs = _numCoeffs;
      if(numCoeffs != 0)
         indexFIRMask = (uint64_t)(powf(2, (ceilf(logf(numCoeffs)/logf(2.0f)))) - 1.000001f);
      }

   void setSampleRate_Hz(const float &fs_Hz) {
      sample_rate_Hz = fs_Hz;
      kp = 512.0f/sample_rate_Hz;
      phaseIncrement[0] = kp*freq[0];
      phaseIncrement[1] = kp*freq[1];
      deltaPhase = kp*(freq[1] - freq[0]);
      samplesPerDataBit = (uint32_t)(0.5 +  sample_rate_Hz/bitRate);
      }

   int16_t checkBuffer(void);
   virtual void update(void);

#define DATA_BUFFER_EMPTY  0
#define DATA_BUFFER_PART   1
#define DATA_BUFFER_FULL   2

private:
   float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE_EXACT;
   uint16_t  block_size = 128;
   float32_t freq[2] = {1300.000f, 2100.000f};
   float32_t kp = 512.0f/sample_rate_Hz;
   float32_t phaseIncrement[2] = {kp*freq[0], kp*freq[1]};
   float32_t deltaPhase = kp*(freq[1] - freq[0]);
   float32_t bitRate = 1200.0f;
   float32_t currentPhase = 0.0f;
   float32_t magnitude = 1.0f;

   uint32_t  currentWord = 0UL;
   uint16_t  numBits = 10;
   uint16_t  bitSendCount = 0;

   uint32_t  dataBuffer[64];     // Make 64 a define??
   // By using a 64-bit index we never need to wrap around.
   // We create a circular 64-word buffer with the mask.
   int64_t  indexIn = 0ULL;         // Next word to be entered to buffer
   int64_t  indexOut = 0ULL;        // Next word to be sent
   int64_t  indexMask = 0X003F;  // Goes with 64
   uint16_t stateBuffer = DATA_BUFFER_EMPTY;
   uint16_t  vc = 0;

   uint32_t  samplePerBitCount = 0;
   // The next for 1200 bit/sec and 44100 sample rate is 37 samples,
   // or actually 1192 bits/sec.
   // For 9600 bit/sec and 96000 sample rate is 10 samples.
   uint32_t  samplesPerDataBit = (uint32_t)(0.5 +  sample_rate_Hz/bitRate);

   float32_t* FIRdata = NULL;
   float32_t* FIRcoeff = NULL;
   uint16_t   numCoeffs = 0;
   uint64_t   indexFIRlatest = 0;  // Next word to be entered to buffer
   uint64_t   indexFIRMask = 0X001F;  // Goes with 32
};
#endif

