/*------------------------------------------------------------------------------------
   AudioAlignLR_F32.cpp

   Author:    Bob Larkin  W7PUA
   Date:      28 Feb 2022

   See AudioAlignLR_F32.h for notes.

  Copyright (c) 2022  Robert Larkin
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
 ------------------------------------------------------------------------------- */

#include "AudioAlignLR_F32.h"

void AudioAlignLR_F32::update(void)  {
   audio_block_f32_t *block_i,*block_q;
   audio_block_f32_t *blockOut_i,*blockOut_q;
   audio_block_f32_t *blockOut_2;
   uint16_t i, j, k;
   // uint32_t t0 = micros();  // Measure time

   if(currentTPinfo.TPstate == TP_IDLE) return;

   block_i = AudioStream_F32::receiveWritable_f32(0);
   if (!block_i) return;

   block_q = AudioStream_F32::receiveWritable_f32(1);
   if (!block_q)  {
      AudioStream_F32::release(block_i);
      return;
      }

   blockOut_i = AudioStream_F32::allocate_f32();
     if (!blockOut_i)
        {
        AudioStream_F32::release(block_i);
        AudioStream_F32::release(block_q);
        return;
        }

   blockOut_q = AudioStream_F32::allocate_f32();
     if (!blockOut_q)
        {
        AudioStream_F32::release(block_i);
        AudioStream_F32::release(block_q);
        AudioStream_F32::release(blockOut_i);
        return;
        }
   blockOut_2 = AudioStream_F32::allocate_f32();
     if (!blockOut_2)
        {
        AudioStream_F32::release(block_i);
        AudioStream_F32::release(block_q);
        AudioStream_F32::release(blockOut_i);
        AudioStream_F32::release(blockOut_q);
        return;
        }

   // One of these may be needed. They are saved for next update
   TPextraI = block_i->data[127];
   TPextraQ = block_q->data[127];

   // Find four cross-correlations for time shifted L-R combinations
   if(currentTPinfo.TPstate==TP_MEASURE)
      {
      currentTPinfo.xcVal[0]=0.0f;   // In phase
      currentTPinfo.xcVal[1]=0.0f;   // Shift I
      currentTPinfo.xcVal[2]=0.0f;   // DNApply, shift 2 time slots
      currentTPinfo.xcVal[3]=0.0f;   // Shift Q
      for(j=0; j<4; j++)
         {
         for(k=0; k<124; k++)    // Use sum of 124 x-c values
            {
            currentTPinfo.xcVal[j] += block_i->data[k] * block_q->data[k+j];
            }
         currentTPinfo.xNorm = 0.0f;
         for(k=0; k<4; k++)
            currentTPinfo.xNorm += fabsf( currentTPinfo.xcVal[k] );
         }

      // Decision time. Still in Measure. Can we leave? Need one more update()?
      // Sort out the offset that is cross-correlated
      if(currentTPinfo.nMeas>5 && currentTPinfo.xNorm > 0.0001f)  // Get past junk at startup
         {
         currentTPinfo.TPerror = ERROR_TP_NONE;  // Change later if not true
         needOneMore = true;  // Change later if not true

         // Redo (12 March 2022) with normalized values
         float32_t  xcN[4];
         for(j=0; j<4; j++)
            xcN[j] = currentTPinfo.xcVal[j]/currentTPinfo.xNorm;
         // Look for a good cross-correlation with the normalized values
         if(xcN[0] - xcN[2] > 0.75f)
            currentTPinfo.neededShift = 0;
         else if(xcN[1] - xcN[3] > 0.75f)
            currentTPinfo.neededShift = 1;
         else if(xcN[3] - xcN[1] > 0.75f)
            currentTPinfo.neededShift = -1;
         else  // Don't have a combination awith much cross-correlation
            {
            currentTPinfo.neededShift = 0;  //  Just a guess
            currentTPinfo.TPerror = ERROR_TP_BAD_DATA;  // Probably no, or low signal
            needOneMore = false;   // Not yet
            }
         }

      if(currentTPinfo.nMeas>5 && needOneMore==false && currentTPinfo.TPerror==ERROR_TP_NONE)
         {
         needOneMore = true;  // Last may have been partial data set
         }
      else if(needOneMore==true && currentTPinfo.TPerror==ERROR_TP_NONE)  // We're done measuring
         {
         currentTPinfo.TPstate = TP_RUN;  // Not TP_MEASURE. State doesn't change from here on.
         needOneMore = false;
         if(currentTPinfo.TPsignalHardware == TP_SIGNAL_CODEC)
            digitalWrite(controlPinNumber, 0 ^ (uint16_t)controlPinInvert);  // Stop test audio
         // Serial.println("Stop Square Wave audio path");
         }
      else  // Try again from the start
         {
         // Serial.println("Re-start TP Measure");
         currentTPinfo.TPstate = TP_MEASURE;
         if(currentTPinfo.TPsignalHardware==TP_SIGNAL_CODEC)
            digitalWrite(controlPinNumber, 1 & (uint16_t)controlPinInvert);
         currentTPinfo.neededShift = 0;
         currentTPinfo.TPerror = ERROR_TP_EARLY;
         needOneMore = false;
         // Unless a reason for sending bad data come up, we will not send it:
         //AudioStream_F32::transmit(block_i, 0);
         //AudioStream_F32::transmit(block_q, 1);
         }
      }       // End state==TP_MEASURE

   else if(currentTPinfo.TPstate == TP_RUN)
      {
      if(currentTPinfo.neededShift == 0)
         {
         // Serial.println("No shift");
         AudioStream_F32::transmit(block_i, 0);         // Not shifted
         AudioStream_F32::transmit(block_q, 1);         // Not shifted
         }
      else if(currentTPinfo.neededShift == 1)
         {
         // Serial.println("Shift 1");
         blockOut_i->data[0] = TPextraI;  // From last update
         // block_i->data[127] is saved for next update, and not
         // transmitted now.
         for(i=1; i<128; i++)
            blockOut_i->data[i] = block_i->data[i-1];
         AudioStream_F32::transmit(blockOut_i, 0);     // Shifted
         AudioStream_F32::transmit(block_q, 1);        // Not shifted
         }
      else if(currentTPinfo.neededShift == -1)
         {
         // Serial.println("Shift -1");
         blockOut_q->data[0] = TPextraQ;
         for(i=1; i<128; i++)
            blockOut_q->data[i] = block_q->data[i-1];
         AudioStream_F32::transmit(block_i, 0);         // Not shifted
         AudioStream_F32::transmit(blockOut_q, 1);      // Shifted
         }
      }      // End state==TP_RUN


      // TP_MEASURE needs a fs/4 test signal
      if(currentTPinfo.TPstate == TP_MEASURE &&
           currentTPinfo.TPsignalHardware == TP_SIGNAL_CODEC)
         {
         for(int kk=0; kk<128; kk++)     // Generate fs/4 square wave
            {
            // A +/- 0.8 square wave at fs/4 Hz
            blockOut_2->data[kk] = -0.8+1.6*(float32_t)((kk/2)&1);
            }
         AudioStream_F32::transmit(blockOut_2, 2); // NOTE: Goes to third output
         }

   currentTPinfo.nMeas++;
   AudioStream_F32::release(block_i);
   AudioStream_F32::release(block_q);
   AudioStream_F32::release(blockOut_i);
   AudioStream_F32::release(blockOut_q);
   AudioStream_F32::release(blockOut_2);
   // Serial.println(micros() - t0);     // for timing
   }

