/*
 * UART_F32.cpp
 *
 * 10 JUne 2022 - Separated from FM Discriminator
 * Bob Larkin, in support of the library:
 * Chip Audette, OpenAudio, Apr 2017
 *     -------------------
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
 *
 * See UART_F32.h for usage details
*/
#include "UART_F32.h"

void UART_F32::update(void) {
   audio_block_f32_t  *blockIn;
   int i;
   bool downTransition;
   float32_t dataIn;

   // For measuring timing
   //  uint32_t t1 = micros();

   // Get input to UART block
   blockIn = AudioStream_F32::receiveWritable_f32(0);
   if (!blockIn) {
      return;
      }

   /* Simple UART  -  Start idle state.  Wait for Start Bit, wait 0.5 bit periods
    * and read Start bit. Read next data bits every cTauI samples.  After last data bit
    * is read, go back to idle state.  This gives a half bit period to allow for fast
    * sending clocks.
    */
   for (i=0; i<block_size; i++)
      {
      sClock++;      // Endless 32 bit count of audio sample periods (27 hours at 44.1 kHz)
      dataIn = blockIn->data[i] + inputOffset;  // Programmable offset, if needed
      downTransition = (dataIn<=0.0f && yLast>0.0f);
      yLast = dataIn;

      if(serialState==SERIAL_DATA_FINISHED)  // Have data bits, waiting to read stop bit
         {           //Serial.print(sClock); Serial.print("sc next"); Serial.println(cSampleNext);
         if(sClock>=cSampleNext)
            {          // Serial.print(" @"); Serial.print(serialState); Serial.println("@");
            if(dataIn > 0.0f)   // Valid Stop Bit
               {
               // Serial.println("P");   // Stop bit is correct
               writeUartData(dataWord, saveStatus, elapsedTime);
               saveStatus &= ERR_OVERRUN; // Error indicator has been sent, don't clear overrun
               }
            else                           // Wrong logic level
               {
               // Many low S/N frame errors are just bit-read errors and not timing errors.
               // No way to tell which, and best to just accept data.
               // Serial.println("F");    // Data has been read with Framing error
               saveStatus |= ERR_FRAME;
               writeUartData(dataWord, saveStatus, elapsedTime);
               saveStatus &= ERR_OVERRUN; // Error indicator has been sent, don't clear overrun
               }
            serialState = SERIAL_IDLE;    // Return to waiting for start bit
            bitCount = 0;
            bitMask = 1UL;
            dataWord = 0UL;
            elapsedTime = 0;
            }
         }
      else if(serialState==SERIAL_DATA)
         {
         if(sClock>=cSampleNext)
            {
            if(bitCount==0)    // Going to read start bit
               {
               if(dataIn<=0.0f)  // Valid start bit
                  {
                  //Serial.print("T");
                  // Logic is low at data read time for valid start bit
                  bitCount = 1;
                  bitMask = 1UL;
                  dataWord = 0UL;
                  cSampleNext += cTauI;
                  }
               else        // Not low, must be noise, cancel out
                  {
                  serialState = SERIAL_IDLE;
                  bitCount = 0;
                  cSampleNext = 0;
                  }
               }
            else  // bitCount>0 so reading data bits
               {
               if(dataIn >= 0.0f)
                  {
                  // Serial.print("1"); // Data bit is a 1
                  dataWord |= bitMask;
                  }
               else
                  {
                  // Serial.print("0"); // Data bit
                  // Data bit is already zero
                  }
               bitMask = (bitMask << 1);
               cSampleNext += cTauI;
               bitCount++;
               if(bitCount>=nTotal)   // Last data bit
                  {
                  serialState = SERIAL_DATA_FINISHED;  // Look for valid stop bit
                  bitCount = 0;
                  }
               }
            }
         }
      else if(serialState==SERIAL_IDLE)   // Waiting for a Start Bit
         {
         if(downTransition)   // Down going, potential start bit
            {
            serialState = SERIAL_DATA;
            timeStartBit = sClock;
            cSampleNext = sClock + cTauHalfI;    // This will sample start bit
            bitCount = 0;
            }
         }

      // UART done, now gather data for checking clock accuracy
      // Use timeStartBit and time of last down transitions.
      // Up transitions may have a bias and are not used.
      if(downTransition)
         {
         elapsedTime = (int32_t)(sClock - timeStartBit);
         }
      }

   AudioStream_F32::release(blockIn);

//   Serial.print("At end of UART ");  Serial.println (micros() - t1);
}
