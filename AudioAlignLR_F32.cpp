
/*------------------------------------------------------------------------------------
   AudioAlignLR_F32.cpp                             3-21-2022

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
   audio_block_f32_t *block_L,*block_R;
   audio_block_f32_t *block2_L,*block2_R;
   audio_block_f32_t *blockOutTestSignal;
   uint16_t i, j, k;
   // uint32_t t0 = micros();  // Measure time

   if(currentTPinfo.TPstate == TP_IDLE) return;

   block_L = AudioStream_F32::receiveWritable_f32(0);
   if (!block_L) return;

   block_R = AudioStream_F32::receiveWritable_f32(1);
   if (!block_R)  {
      AudioStream_F32::release(block_L);
      return;
      }

   block2_L = AudioStream_F32::allocate_f32();
     if (!block2_L)
        {
        AudioStream_F32::release(block_L);
        AudioStream_F32::release(block_R);
        return;
        }

   block2_R = AudioStream_F32::allocate_f32();
     if (!block2_R)
        {
        AudioStream_F32::release(block_L);
        AudioStream_F32::release(block_R);
        AudioStream_F32::release(block2_L);
        return;
        }

   blockOutTestSignal = AudioStream_F32::allocate_f32();
     if(currentTPinfo.TPsignalHardware==TP_SIGNAL_CODEC  &&  !blockOutTestSignal)
        {
        AudioStream_F32::release(block_L);
        AudioStream_F32::release(block_R);
        AudioStream_F32::release(block2_L);
        AudioStream_F32::release(block2_R);
        return;
        }

   // Input data is now in block_L and block_R.  Filter from there to
   // block2_L and block2_R
   if(useLRfilter)
      {
      arm_fir_f32(&fir_instL, block_L->data, block2_L->data, block_L->length);
      arm_fir_f32(&fir_instR, block_R->data, block2_R->data, block_R->length);
      }
    else
       for(i=0; i<block_L->length; i++)
          {
          block2_L->data[i] = block_L->data[i];
          block2_R->data[i] = block_R->data[i];
          }
   // Input data is now in block_L and block_R.  Filtered data is in
   // block2_L and block2_R

   // One of these next 2 may be needed. They are saved for next update
   TPextraL = block_L->data[127];
   TPextraR = block_R->data[127];

   // Find four cross-correlations for time shifted L-R combinations.
// Use filtered data
   if(currentTPinfo.TPstate==TP_MEASURE)
      {
      currentTPinfo.xcVal[0]=0.0f;   // In phase
      currentTPinfo.xcVal[1]=0.0f;   // Shift I
      currentTPinfo.xcVal[2]=0.0f;   // DNApply, shift 2 time slots
      currentTPinfo.xcVal[3]=0.0f;   // Shift Q
      for(j=0; j<4; j++)
         {
         for(k=0; k<124; k++)    // Use sum of 124 x-c values on filtered data
            {
            currentTPinfo.xcVal[j] += block2_L->data[k] * block2_R->data[k+j];
            }
         }
     currentTPinfo.xNorm = 0.0f;
         for(k=0; k<4; k++)
            currentTPinfo.xNorm += fabsf( currentTPinfo.xcVal[k] );

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
         }
      }       // End state==TP_MEASURE

   else if(currentTPinfo.TPstate == TP_RUN)
      {
      if(currentTPinfo.neededShift == 0)
         {
         // Serial.println("No shift");
         }
      else if(currentTPinfo.neededShift == 1)
         {
         // Serial.println("Shift 1");
         for(i=127; i>0; i--)
            block_L->data[i] = block_L->data[i-1];  // Move all down one
         block_L->data[0] = TPextraL;  // From last update
         // Note: block_L->data[127] is saved for next update, and not
         // transmitted now.
         }
      else if(currentTPinfo.neededShift == -1)
         {
         // Serial.println("Shift -1");
         for(i=127; i>0; i--)
            block_R->data[i] = block_R->data[i-1];
         block_R->data[0] = TPextraR;
         }
      // Only transmit data in TP_RUN
      AudioStream_F32::transmit(block_L, 0);     // Shifted
      AudioStream_F32::transmit(block_R, 1);     // as needed
      }      // End state==TP_RUN

   // But, always release the blocks
   AudioStream_F32::release(block_L);
   AudioStream_F32::release(block_R);
   AudioStream_F32::release(block2_L);
   AudioStream_F32::release(block2_R);

   // TP_MEASURE needs a fs/4 test signal
   if(currentTPinfo.TPsignalHardware == TP_SIGNAL_CODEC)
      {
      if(currentTPinfo.TPstate == TP_MEASURE)
         {
         for(int kk=0; kk<128; kk++)     // Generate fs/4 square wave
            {
            // A +/- 0.8 square wave at fs/4 Hz
            blockOutTestSignal->data[kk] = -0.8+1.6*(float32_t)((kk/2)&1);
            }
         }
      AudioStream_F32::transmit(blockOutTestSignal, 2); // NOTE: Goes to third output
      }
   AudioStream_F32::release(blockOutTestSignal);

   currentTPinfo.nMeas++;
   // Serial.println(micros() - t0);     // for timing
   }
