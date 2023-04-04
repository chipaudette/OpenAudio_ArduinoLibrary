/*
 * radioBFSKmodulator_F32
 *
 * Created: Bob Larkin 17 March 2022
 *
 * License: MIT License. Use at your own risk. See corresponding .h file.
 *
 * 2 April 2023 -Corrected to handle outputs from a full buffer.  RSL
 *               Added    int16_t getBufferSpace().  RSL
 */

#include "radioBFSKmodulator_F32.h"
// 513 values of the sine wave in a float array:
#include "sinTable512_f32.h"

// 64 entry send buffer check. Sets stateBuffer
// Returns the number of un-transmitted characters.
int16_t RadioBFSKModulator_F32::checkBuffer(void)  {
   int64_t deltaIndex = indexIn - indexOut;

   if(deltaIndex==0LL)
      stateBuffer = DATA_BUFFER_EMPTY;
   else if(deltaIndex==64LL)
      stateBuffer = DATA_BUFFER_FULL;
   else
      stateBuffer = DATA_BUFFER_PART;

   if(deltaIndex<0LL || deltaIndex>64LL)    // Should never happen
      {
      Serial.print("ERROR in Buffer;  deltaIndex = ");
      Serial.println(deltaIndex);
      }
   return (int16_t)deltaIndex;
   }

void RadioBFSKModulator_F32::update(void) {
   audio_block_f32_t *blockFSK;
   uint16_t index, i;
   float32_t vca;
   float32_t a, b;
   // uint32_t tt=micros();

   blockFSK = AudioStream_F32::allocate_f32();   // Get the output block
   if (!blockFSK)  return;

   for (i=0; i < blockFSK->length; i++)
      {
     samplePerBitCount++;
      // Each data bit is an integer number of sample periods
      if(samplePerBitCount >= samplesPerDataBit)  // Bit transition time
         {
         samplePerBitCount = 0;       // Wait a bit period before checking again
         if(bitSendCount < numBits)   // Still sending a word, get next bit
            {
            vc = (uint16_t)(currentWord & 1);   // Send 0 or 1
            currentWord = currentWord >> 1;     // Next bit position
            bitSendCount++;
            }
         else    // End of word, get another if possible
            {
            if(stateBuffer != DATA_BUFFER_EMPTY)
               {
               indexOut++;   // Just keeps on going, not circular
               checkBuffer();
               currentWord = dataBuffer[indexOut&indexMask];
//             Serial.print(indexIn);
//             Serial.print(" In   Out ");
//             Serial.println(indexOut);
               vc = (uint16_t)(currentWord & 1);  // Bit to send next
               bitSendCount = 1U;   // Count how many bits have been sent
               currentWord = currentWord >> 1;
               }
            else           // No word available
               {
               vc = 1;     //Idle at logic 1 (Make programmable?)
               }
            }     // end, get another word
         }       // end, bit transition time


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

      // Modulate baseband vca voltage to sine wave frequency (VCO)
      // pre-multiply phaseIncrement[0]=kp*freq[0]  and deltaPhase=kp*deltaFreq
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

