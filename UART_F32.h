/*
 * UART_F32
 * 10 JUne 2022 - Separated from FM Discriminator
 *
 * Copyright (c) 2022 Bob Larkin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* This object takes a single audio input representing the baseband output
 * of a suitable detector such as frequency or phase.  The polarity of the
 * input is >0.0 for logic 1 as well as the the idle state and <0.0 for logic 0.
 * The timing of the UART is specified by the
 *   setUART(cTauI, cTauHalfI, nBits, nParity, nStop) function.  The first two
 * parameters are the data bit period measured in audio sample periods,
 * and half of that (roughly) that sets the number of audio sample periods between
 * between the 1-to-0 transition of the start bit and the middle of the start bit.
 * nBits can be between 1 and 32 (default 8). nParity can be PARITY_NONE,
 * PARITY_ODD or PARITY_EVEN. nStop is currently restricted to 1, but
 * this could change.
 *
 * Data is read using the functions
 *   int32_t getNDataBuffer() that returns the number of words available
 * and readUartData() that returns a pointer to the oldest UART data structure:
 *   struct uartData {
 *      uint32_t data;
 *      uint8_t error;    // Parity=01, Overrun=02, Underrun=04
 *      int32_t timeCrossings;}
 * Up to 16 of these structures can be buffered before creating
 * an ERR_OVERRUN.
 *
 * NOTE: Parity checking does nothing now.  Needs to be added using
 * the in-place function.  June 2022
 *
 * Time:    For T4.0, 7 microseconds for 128 data points.
 *
 */

#ifndef _uart_f32_h
#define _uart_f32_h

#include "AudioStream_F32.h"
#include "OpenAudio_ArduinoLibrary.h"

#define SERIAL_IDLE 0
#define SERIAL_DATA 1
#define SERIAL_DATA_FINISHED 2

#define PARITY_NONE 0
#define PARITY_ODD 1
#define PARITY_EVEN 2

#define ERR_FRAME 1;
#define ERR_OVERRUN 2
#define ERR_PARITY 4

struct uartData {
   uint32_t data;
   uint8_t  status;
   int32_t  timeCrossings;
   };

class UART_F32 : public AudioStream_F32 {
//GUI: inputs:1, outputs:0  //this line used for automatic generation of GUI node
//GUI: shortName: uart
public:
   // Default block size and sample rate:
   UART_F32(void) :  AudioStream_F32(1, inputQueueArray_f32) {
   }
   // Option of AudioSettings_F32 change to block size and/or sample rate:
   UART_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) {
      sampleRate_Hz = settings.sample_rate_Hz;
      block_size = settings.audio_block_samples;
   }

   void setUART(uint32_t  _cTauI, uint32_t _cTauHalfI,
                uint16_t  _nBits, uint16_t _nParity, uint16_t  _nStop) {
   cTauI = _cTauI;
   cTauHalfI = _cTauHalfI;
   nBits = _nBits;
   nStop = _nStop;
   nTotal = 1 + nBits;
   if(nParity > 0)   nTotal++;
   }

   // Returns the number of unread data words
   int32_t getNDataBuffer(void)  {
	  delay(1);             // Why needed?? <<<<<<<<<<<<<<
      return (uartWriteIndex - uartReadIndex);
      }

   // Read UART data returns a pointer to the uartData structure.
   // This increments the index and thus can only be called once per
   // successful UART output word.  If no data is available, a NULL
   // pointer is returned.
   struct uartData* readUartData(void)  {
      if(uartReadIndex >= uartWriteIndex)  // Never should be greater
         return NULL;
      int32_t uri = uartReadIndex & uartDMask;
      // Circular increment of the read index
      uartReadIndex++;
      bufferSpace = 16 - uartWriteIndex + uartReadIndex;
      return &uartD[uri];
      }

   // The input is a floating point value, centered on 0.0.  One source of this
   // could be a discriminator base-band output.  If the input signal is offset
   // from zero, it can be corrected with this function.  This inputOffset is
   // added to the input and can be + or - in value.
   void setInputOffset(float32_t _inputOffset) {
      inputOffset = _inputOffset;
      }

   void setSampleRate_Hz(float32_t _sampleRate_Hz) {
      sampleRate_Hz = _sampleRate_Hz;
      }

   virtual void update(void);

private:
   // One input data pointer
   audio_block_f32_t *inputQueueArray_f32[1];
   float32_t sampleRate_Hz = AUDIO_SAMPLE_RATE_EXACT;
   uint16_t block_size = AUDIO_BLOCK_SAMPLES;

   // BFSK decode clock variables
   // Always 1 start bit.
   uint16_t  nBits = 8;          // Data bits
   uint16_t  nParity = PARITY_NONE;
   uint16_t  nStop = 1;          // Fixed for now
   uint16_t  nTotal = 9;         // 1+nBits+Parity
   uint32_t  cTauI = 40UL;
   uint32_t  cTauHalfI = 20UL;

   struct uartData uartD[16];   // Circular buffer of UART data

   float32_t inputOffset = 0.0f;  // Offset of input data

   // These next 2 indices are the index, mod 16, where we wil write
   // data to next, or read data from next (if available).
   // If the two are equal, no data is available to read.
   int32_t   uartReadIndex = 0L;  // Both indices continue to grow, unbounded
   int32_t   uartWriteIndex = 0L;

   const int32_t uartDMask = 0B1111; // Mask for 16 word buffer
   int32_t   bufferSpace = 16L; // 16 - uartWriteIndex + uartReadIndex
   uint16_t  serialState = SERIAL_IDLE;
   uint8_t   saveStatus;
   float32_t yData = 0.0f;
   float32_t yLast = -0.01f;
   float32_t yClock = -1.0f;
   float32_t errorClock = 0.0f;
   uint32_t  sClock = 0UL;   // Counted in audio sample units
   uint32_t  lastTime = 0UL;  // Last zero crossing
   uint32_t  timeStartBit= 0UL;
   uint32_t  timeLastTransition = 0UL;
   uint32_t  cSampleNext = 0UL;
   uint8_t   bitCount = 0UL;   // Where we are at in receiving word
   uint32_t  bitMask = 1;      // Values 1, 2, 4, 8, 16,...  tracks bitCount
   uint32_t  dataWord = 0UL;
   uint32_t  elapsedTime = 0UL;

void initUartData(uint n) {
   uartD[n].data = 0UL;
   uartD[n].status = 0;          // including data ready, overrun, parity and framing errors
   uartD[n].timeCrossings = 0;
   }

// Write an output word for Serial BFSK modem
void writeUartData(uint32_t _data, uint8_t _status, int32_t _timeCrossings)  {
   bufferSpace = 16L - uartWriteIndex + uartReadIndex;  //0 to 16 amount of space available
   if(bufferSpace > 0)
      {
      uartD[uartWriteIndex&uartDMask].data = _data;
      uartD[uartWriteIndex&uartDMask].status = _status;
      uartD[uartWriteIndex&uartDMask].timeCrossings = _timeCrossings;
      uartWriteIndex++;    // Bump write index
      saveStatus &= ~ERR_OVERRUN;   // Clear overrun bit, it has finally been sent
      }
   else      // No room in buffer
      {
      saveStatus |= ERR_OVERRUN;  // Error gets sent when overrun is cleared
      }
   bufferSpace = 16L - uartWriteIndex + uartReadIndex;
   }

// Thanks svicent.   NEEDS TO BE ADDED TO  writeUartData() <<<----
uint8_t oddParity(uint32_t ino)  {
   uint8_t n8 = 0;
   while(ino != 0)
      {
      n8++;
      ino &= (ino-1); // the loop will execute once for each bit of ino set
      }
   /* if n8 is odd, least significant bit will be 1 */
   return (n8 & 1);
   }

};   // End class UART_F32
#endif
