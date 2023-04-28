/*
*   radioCWModulator_F32.cpp
*
*   Created: Bob larkin  W7PUA  March 2023
*
*   License: MIT License.  Use at your own risk.
*/

#include "radioCWModulator_F32.h"
#include "sinTable512_f32.h"

void radioCWModulator_F32::update(void)  {
   float32_t keyData[128];     // CW key down and up, 0.0f and 1.0f
   float32_t modulateCW[128];  // Storage for data to modulate sine wave
   uint16_t index, i;
   float32_t a, b;
   audio_block_f32_t *blockOut;

   blockOut = AudioStream_F32::allocate_f32();   // Output block
   if (!blockOut)  return;

   // A new character cannot enter sendBuffer during an interrupt, but
   // the state IDLE_CW can be created by some other state ending.
   // So it needs to be in the audio sample loop.

   // We always generate CW at 12 ksps.  The number of data points in this
   // generation varies to provide 128 output output points after
   // interpolation to 48 or 96 ksps.
    for(i=0; i<nSamplesPerUpdate; i++)
      {
      timeMsF += timeSamplesMs;
      timeMsI = (uint32_t)(0.5 + timeMsF);

      if(!enableXmit)   // Just leave the key up and no new characters
         {
         levelCW = 0.0f;
         goto noXmit;
         }

      switch(stateCW)
         {
         case IDLE_CW:
            timeMsF = 0.0f;
            timeMsI = 0;
            if(((indexW-indexR)&0X1FF) > 0)   // Get next char, if available
               {
               c = sendBuffer[indexR];
               if (c>95)
                  c -= 64;        // Convert lc to caps
               else
                  c -= 32;        // Move subscript to (0, 63)
               ic = mc[(int)c];   // Ch to morse code lookup
             if (c==27)         // Long Dash from ';'
                  {
                  stateCW = LONG_DASH;
                  levelCW = 1.0f;
                  timeMsF = 0.0f;
                  timeMsI = 0;
                  }
              else if (ic==0X17)   // A space character
                  {
                  stateCW = WORD_SPACE;
                  levelCW = 0.0f;
                  timeMsF = 0.0f;
                  timeMsI = 0;
                  }
               else if(ic>1 && (ic & 1)==0x01)
                  {
                  stateCW = DASH_CW;
                  levelCW = 1.0f;
                  timeMsF = 0.0f;
                  timeMsI = 0;
                  }
               else if(ic>1 && (ic & 1)==0X00)
                  {
                  stateCW = DOT_CW;
                  levelCW = 1.0f;
                  timeMsF = 0.0f;
                  timeMsI = 0;
                  }
               else if(ic==0X01)
                  {
                  stateCW = IDLE_CW;
                  levelCW = 0.0f;
                  }
               }     // end, if new character
            break;
         case DASH_CW:
            if(timeMsI > dashCW)   // Finished dash
               {
               levelCW = 0.0f;
               if(ic>1) ic >>= 1;  // Shift right  1
               if(ic==1)
                  stateCW = CHAR_SPACE;
               else
                  stateCW = DOT_DASH_SPACE;
               timeMsF = 0.0f;
               timeMsI = 0;
               }
            break;
         case DOT_CW:
             if(timeMsI > dotCW)
               {
               levelCW = 0.0f;
               if(ic>1) ic >>= 1;  // Shift right  1
               if(ic==1)
                  stateCW = CHAR_SPACE;
               else
                  stateCW = DOT_DASH_SPACE;
               timeMsF = 0.0f;
               timeMsI = 0;
               }
             break;
         case DOT_DASH_SPACE:
            if(timeMsI > ddCW)  // Just finished
               {
               timeMsF = 0.0f;
               timeMsI = 0;
                if(ic>1 && (ic & 1)==0x01)
                  {
                  stateCW = DASH_CW;
                  levelCW = 1.0f;
                  }
               else if(ic>1 && (ic & 1)==0X00)
                  {
                  stateCW = DOT_CW;
                  levelCW = 1.0f;
                  }
               else
                  {
                  stateCW = IDLE_CW;
                  levelCW = 0.0;
                  }
               }
               break;
         case CHAR_SPACE:
             if(timeMsI > chCW+ddCW)  // Just finished sending a character
             // chCW+ddCW sounds better than chCW to me.
               {
               indexR++;  // Sending is ended, bump the read index
               indexR = indexR & 0X1FF;   // Confine to (0, 511)
               timeMsF = 0.0f;
               timeMsI = 0;
               stateCW = IDLE_CW;
               break;
               }
         case WORD_SPACE:
             if(timeMsI > spCW)  // Just finished sending a space
               {
               indexR++;  // Sending is ended, bump the read index
               indexR = indexR & 0X1FF;   // Confine to (0, 511)
               timeMsF = 0.0f;
               timeMsI = 0;
               stateCW = IDLE_CW;
               break;
                }
         case LONG_DASH:
             if(timeMsI > longDashCW)  // Just finished sending a long dash
               {
               levelCW = 0.0f;
               stateCW = CHAR_SPACE;
               timeMsF = 0.0f;
               timeMsI = 0;
               break;
               }
      }   // end switch
   noXmit:
   keyData[i] = levelCW;
   }      // end, over all 128 times

   arm_fir_f32(&GaussLPFInst, keyData, keyData, nSamplesPerUpdate);

   // INTERPOLATE -   Interpolate here to support higher sample rates,
   // while using the same spectral LPF.  To this point we have 128, 32
   // or 16 "active" data points for 12, 48, or 96ksps.
   //
   // 0  1  2  3  4  5  6  7  8  9   i
   // 0  0  0  0  1  1  1  1  2  2   i/4
   // t  0  0  0  t  0  0  0  t  0   i==4*(i/4)
   //
   if(nSample > 1)      // Only needs interpolation if >1
      {
      for(i=0; i<128; i++)
         {
         if( i==(nSample*(1/nSample)) )
            modulateCW[i]= keyData[i/nSample];
         else
            modulateCW[i] = 0.0f;
         }
      arm_fir_f32(&interpolateLPFInst, modulateCW, keyData, 128);
      }

   // Interpolation is done, now amplitude modulate CW onto a sine wave.
   for (i=0; i < 128; i++)
      {
      phaseS += phaseIncrement;
      if (phaseS > 512.0f)  phaseS -= 512.0f;
      index = (uint16_t) phaseS;
      float32_t deltaPhase = phaseS - (float32_t)index;
      // Read two nearest values of input value from the sine table
      a = sinTable512_f32[index];
      b = sinTable512_f32[index+1];
      blockOut->data[i] = magnitude*keyData[i]*(a+(b-a)*deltaPhase);
      }
   AudioStream_F32::transmit(blockOut);
   AudioStream_F32::release (blockOut);
   }
