/*
 * radioBFSKmodulator_F32
 *
 * Created: Bob Larkin 17 March 2022
 *
 * License: MIT License. Use at your own risk. See corresponding .h file.
 *
 */

#include "radioBFSKmodulator_F32.h"
// 513 values of the sine wave in a float array:
#include "sinTable512_f32.h"

void RadioBFSKModulator_F32::update(void) {
   audio_block_f32_t *blockFSK;
   uint16_t index, i;
   float32_t vca;
   float32_t a, b;

   // uint32_t tt=micros();

   blockFSK = AudioStream_F32::allocate_f32();   // Get the output block
   if (!blockFSK)  return;

   // Note - If buffer is dry, there is a delay of up to 3 mSec.  Should be OK

   // Note: indexIn and indexOut will not change during this update(),
   // as we have the interrupt.

   if(atIdle && (indexIn - indexOut) > 0)  // At idle and new buffer entry has arrived
      {
      atIdle = false;
      bitSendCount = numBits + 1;   // Force new start
      samplePerBitCount = samplesPerDataBit + 1;
      }

   for (i=0; i < blockFSK->length; i++)
      {
      // Each data bit is an integer number of sample periods
      if(samplePerBitCount >= samplesPerDataBit)  // Time for either idle or new bit/word
         {
         if(bitSendCount < numBits)   // Still sending a word, get next bit
            {
            vc = (uint16_t)(currentWord & 1);
            currentWord = currentWord >> 1;
            bitSendCount++;
            samplePerBitCount = 0;
            }
         else if((indexIn - indexOut) > 0)  // Is there another word ready?
            {
            atIdle = false;
            indexOut++;   // Just keeps on going, not circular
            currentWord = dataBuffer[indexOut&indexMask];
            vc = (uint16_t)(currentWord & 1);  // Bit to send next
            bitSendCount = 1U;   // Count how many bits have been sent
            currentWord = currentWord >> 1;
            samplePerBitCount = 0;
            }
         else           // No bits are available
            {
            atIdle = true;
            vc = 1;     //Idle at logic 1 (Make programmable?)
            samplePerBitCount = samplesPerDataBit + 1;  // Force revisit
            }
         }

      if(FIRcoeff==NULL)    //  No LPF being used
         vca = (float32_t)vc;
      else
         {
         // Put latest data point intoFIR LPF buffer
         indexFIRlatest++;
         FIRdata[indexFIRlatest & indexFIRMask] = (float32_t)vc;  // Add to buffer
         // Optional FIR LPF
         vca = 0.0f;
         int16_t iiBase16 = (int16_t)(indexFIRlatest & indexFIRMask);
         int16_t iiSize = (int16_t)(indexFIRMask + 1ULL);
         for(int16_t k=0; k<numCoeffs; k++)
            {
            int16_t ii = iiBase16 - k;
            if(ii < 0)
               ii += iiSize;
            vca += FIRcoeff[k]*FIRdata[ii];
            }
         }
      // pre-multiply phaseIncrement[0]=kp*freq[0]  and  deltaPhase=kp*deltaFreq
      currentPhase += (phaseIncrement[0] + vca*deltaPhase);
      if (currentPhase > 512.0f)  currentPhase -= 512.0f;
      index = (uint16_t) currentPhase;
      float32_t deltaPhase = currentPhase - (float32_t)index;
      /* Read two nearest values of input value from the sin table */
      a = sinTable512_f32[index];
      b = sinTable512_f32[index+1];
      blockFSK->data[i] = magnitude*(a+(b-a)*deltaPhase); // Linear interpolation
      samplePerBitCount++;
      }

   AudioStream_F32::transmit(blockFSK);
   AudioStream_F32::release (blockFSK);
   // Serial.print("     "); Serial.println(micros()-tt);
   }
