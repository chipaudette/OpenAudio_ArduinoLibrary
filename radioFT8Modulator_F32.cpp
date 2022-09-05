/*
 * radioFT8modulator_F32
 *
 * Created: Bob Larkin 17 July 2022
 *
 * License: MIT License. Use at your own risk. See radioFT8Modulator.h file.
 *
 */

#include "radioFT8Modulator_F32.h"
// 513 values of the sine wave in a float array:
#include "sinTable512_f32.h"

void RadioFT8Modulator_F32::update(void) {
   audio_block_f32_t *blockOut;
   uint16_t index;
   float32_t a, b;
   float32_t mag = 1.0f;

   // uint32_t tu = micros();

   if(ft8Transmit==false)
      return;
   // Sending tones.  Sort out actions
   blockOut = AudioStream_F32::allocate_f32();   // Get the output block
   if (!blockOut)
      return;

   /* NOTE - ALL FT8 values shown in comments below are for a 48 kHz sample rate
    * tone81SendCount     0 to 80 (81 tones)
    * audioSampleCount    0 to 7679 for 48kHz sample rate
    * sampleSent          0 to 622079 for 48kHz, 12.96 seconds, 81 tones
    */
   // Send 128 audio samples of FSK sine waves
   for(int kk=0; kk<128; kk++)
      {
      audioSampleCount = sampleSent % toneLength;  // audioSampleCount is always derived from sampleSent
      mag = magnitude;    // Use down below
      dphi = phaseIncrement;
      if(tone81SendCount>0 && tone81SendCount<80)  // Exclude end tones as special cases
         {
         if(audioSampleCount<pulseSegActive)   //  First part of tone with pulse shaping
            {
            // Overlap from previous tone with pulse falling 0.499608070 to 0.0 audioSampleCount 0 to 2815
            dphi += (pulse[pulseSegActive - audioSampleCount - 1])*tones[tone81SendCount-1]*dphi_peak;
            // plus current tone with pulse rising 0.500391960 to 1.0, audioSampleCount 0 to 2815
            dphi += (1.0f - pulse[pulseSegActive - audioSampleCount - 1])*tones[tone81SendCount]*dphi_peak;
            }
         else if(audioSampleCount>=pulseSegActive && audioSampleCount<(toneLength - pulseSegActive))  // Center region, no pulses involved
            {
            // Center of tone with pulse level at 1.0, main tone, 2816 to 4863
            dphi += tones[tone81SendCount]*dphi_peak;
            }
         else  //if((audioSampleCount - 1)>(toneLength - pulseSegActive))   //  Last part of tone with pulse shaping
            {
            // Current tone with pulse falling 1.0 to 0.500391960, audioSampleCount 4864 to 7679
            dphi += (1.0f - pulse[audioSampleCount - (toneLength - pulseSegActive)])*tones[tone81SendCount]*dphi_peak;
            // plus overlap from next tone with pulse rising 0.0 to 0.499608070, audioSampleCount 4864 to 7679
            dphi += (pulse[audioSampleCount - (toneLength - pulseSegActive)])*tones[tone81SendCount + 1]*dphi_peak;
            }
         }   // End of middle 79 tones
      else if(tone81SendCount==0)      // Key click pulse with no tone ahead
         {
         if((audioSampleCount)<(toneLength - pulseSegActive))  // audioSampleCount 0 to 4863 incl or 4864 points
            {
            // Pulse level at 1.0, main tone
            dphi += tones[0]*dphi_peak;
            }
         else   //  Last part of tone with pulse shaping
            {
            // Current tone with pulse falling 1.0 to 0.50039196, audioSampleCount 4864 to 7679 or 2816 pts
            dphi += (1.0f - pulse[audioSampleCount -(toneLength - pulseSegActive)])*tones[0]*dphi_peak;
            // plus overlap from next tone with pulse rising 0.00000003 to 0.49960807 on 4864 to 7679
            dphi += (pulse[audioSampleCount - (toneLength - pulseSegActive)])*tones[1]*dphi_peak;
            }
         if(audioSampleCount<cosineRampActive)
            mag = magnitude*ramp[audioSampleCount];
         else
            mag = magnitude;
         }   // End of first tone

      else if(tone81SendCount==80)      // Key click pulse with no tone after
         {
         if(audioSampleCount<pulseSegActive)   //  First part of tone with pulse shaping
            {
            // Overlap from previous tone with pulse falling 0.499608070 to 0,00000
            // over audioSampleCount 0 to 2815
            dphi += (pulse[pulseSegActive - audioSampleCount - 1])*tones[79]*dphi_peak;
            // plus current tone with pulse rising 0.50039196 to 1.0000000  0 to 2815
            dphi += (1.0f - pulse[pulseSegActive - audioSampleCount - 1])*tones[80]*dphi_peak;
            }
         else //(audioSampleCount>=pulseSegActive)  // Rest of region, no pulses involved
            {
            // Rest of tone with pulse level at 1.0, main tone, audioSampleCount 2816 to 7679
            dphi += tones[80]*dphi_peak;
            }
         if(audioSampleCount>(toneLength - cosineRampActive))
            {
            mag = magnitude*ramp[toneLength - audioSampleCount];
            }
         else
            mag = magnitude;
         }  // End of last tone
      currentPhase += dphi;
      while(currentPhase > 512.0f)
         currentPhase -= 512.0f;
      while(currentPhase < 0.0f)
         currentPhase += 512.0f;
      index = (uint16_t) currentPhase;
      float32_t deltaPhase = currentPhase - (float32_t)index;
      // Read two nearest values of input value from the sin table
      a = sinTable512_f32[index];
      b = sinTable512_f32[index+1];

      blockOut->data[kk] = mag*(a+(b-a)*deltaPhase); // Linear interpolation
      sampleSent++;
      audioSampleCount = sampleSent % toneLength;
      if(audioSampleCount == 0)  // End of a tone send
         {
         if(++tone81SendCount == 81)    // End of the 12.96 second period
            {
            tone81SendCount = 0;
            ft8Transmit = false;
            // Serial.println("End Transmission");
            }
         }
      }  // End, over all  128 audio output points
   AudioStream_F32::transmit(blockOut);
   AudioStream_F32::release (blockOut);
   // Serial.println(micros() - tu);
   }


