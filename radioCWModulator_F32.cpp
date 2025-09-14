/*
*   radioCWModulator_F32.cpp
*
*   Created: Bob larkin  W7PUA  March 2023
*
*   License: MIT License.  Use at your own risk.
*/
// Update notes are  in radioCWModulator_F32.h

#include "radioCWModulator_F32.h"
#include "sinTable512_f32.h"

void radioCWModulator_F32::update(void)  {

   if(!enableXmit)
      return;

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
   // interpolation to 24, 48 or 96 ksps.  So for each update(), we generate
   // 128 points if sample rate is 12 ksps and 32 if it is 48 ksps, etc. The
   // remainder are filled in by interpolation, below.
   for(i=0; i<nSamplesPerUpdate; i++)
      {
      // Code is generated at 12 ksps, so always increment by 1000mSec/12000:
      timeMsF += 0.0833333;  // in milliSec
      timeMsI = (uint32_t)(0.5 + timeMsF);
      if(manualCW)
         {
         if(keyDown)
            levelCW = 1.0f;
         else
            levelCW = 0.0f;
         goto noAuto;
         }

      switch(stateCW)      // Sort out the automatic keying
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
   noAuto:
   dataBuf12[i] = levelCW;   // A float, 128 to 16 of them, starting at i=0
   }      // end, over all 128 to 16 times

   if(sampleRate == SR_12KSPS)
      // Do anti-key-click shaping of the 0.0 and 1.0 values.
      arm_fir_f32(&GaussLPFInst, dataBuf12, dataBuf12, 128);
   else
      // Use dataBuf12A for FIR output, 64, 32, or 16 points
      arm_fir_f32(&GaussLPFInst, dataBuf12, dataBuf12A, nSamplesPerUpdate);

   // For SR_12KSPS there is no interpolation

   /* INTERPOLATE -   Interpolate here to support higher sample rates,
   while using the same spectral LPF.  To this point we have 64, 32
   or 16 "active" data points for 24, 48, or 96ksps, in dataBuf12A[].
   */
   if(sampleRate == SR_24KSPS)
      {
      // ToDo:  Re-scale coefficients of FIR x2.0 to eliminate next statement
      for(int kk=0; kk<64; kk++)   dataBuf12A[kk] *= 2.0f;
      arm_fir_interpolate_f32 (&interp12_24Inst, dataBuf12A, dataBuf24, 64);
      }
   else if(sampleRate == SR_48KSPS)
      {
      // ToDo:  Re-scale coefficients of FIR x2.0 to eliminate next statement
      for(int kk=0; kk<32; kk++)   dataBuf12A[kk] *= 4.0f;
      arm_fir_interpolate_f32 (&interp12_24Inst, dataBuf12A, dataBuf24, 32);
      arm_fir_interpolate_f32 (&interp24_48Inst, dataBuf24,  dataBuf48, 64);
      }
   else if(sampleRate == SR_96KSPS)
      {
      // ToDo:  Re-scale coefficients of FIR x2.0 to eliminate next statement
      for(int kk=0; kk<16; kk++)  dataBuf12A[kk] *= 8.0f;
      arm_fir_interpolate_f32 (&interp12_24Inst, dataBuf12A, dataBuf24, 16);
      arm_fir_interpolate_f32 (&interp24_48Inst, dataBuf24,  dataBuf48, 32);
      arm_fir_interpolate_f32 (&interp48_96Inst, dataBuf48,  dataBuf96, 64);
      }

   // Interpolation is complete, now amplitude modulate CW onto a sine wave.
   for (i=0; i < 128; i++)   // Always 128 modulation signals
      {
      float32_t vOut = 0.0;
      if(sampleRate == SR_12KSPS)
         vOut = dataBuf12[i];
      else if(sampleRate == SR_24KSPS)
         vOut = dataBuf24[i];
      else if(sampleRate == SR_48KSPS)
         vOut = dataBuf48[i];
      else if(sampleRate == SR_96KSPS)
         vOut = dataBuf96[i];

      phaseS += phaseIncrement;
      if (phaseS > 512.0f)  phaseS -= 512.0f;
      index = (uint16_t) phaseS;
      float32_t deltaPhase = phaseS - (float32_t)index;
      // Read two nearest values of input value from the sine table
      a = sinTable512_f32[index];
      b = sinTable512_f32[index+1];
      blockOut->data[i] = magnitude*vOut*(a+(b-a)*deltaPhase); 
      }
   AudioStream_F32::transmit(blockOut);
   AudioStream_F32::release (blockOut);
   }   // End update()



