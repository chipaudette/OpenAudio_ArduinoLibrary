/*
 * radioNoiseBlanker_F32.cpp
 *
 * 22 March 2020
 * Bob Larkin, in support of the library:
 * Chip Audette, OpenAudio, Apr 2017
 *     -------------------
 *
 * MIT License,  Use at your own risk.
*/

#include "radioNoiseBlanker_F32.h"

void radioNoiseBlanker_F32::update(void) {
  audio_block_f32_t *blockIn0=NULL;
  audio_block_f32_t *blockOut0=NULL;
  audio_block_f32_t *blockIn1=NULL;
  audio_block_f32_t *blockOut1=NULL;
  uint16_t i;
  float32_t absSignal;

  // Get input block   // <<Writable??
  blockIn0 = AudioStream_F32::receiveWritable_f32(0);
  if (!blockIn0) {
     // if(errorPrint)  Serial.println("NB-ERR: No input memory");
     return;
  }

  if(twoChannel)  {
     blockIn1 = AudioStream_F32::receiveWritable_f32(1);
     if (!blockIn1) {
        AudioStream_F32::release(blockIn0);
        if(errorPrint)  Serial.println("NB-ERR: No 1 input memory");
        return;
     }
  }
  
  // Are we not noise blanking?
  if(! runNB) {
      AudioStream_F32::transmit(blockIn0, 0); // send the unchanged data
      AudioStream_F32::release (blockIn0);
      if(twoChannel) {
         AudioStream_F32::transmit(blockIn1, 1);
         AudioStream_F32::release (blockIn1);
      }
      return;
  }

  // Get a block for the output
  blockOut0 = AudioStream_F32::allocate_f32();
  if (!blockOut0) {      // Didn't have any
    if(errorPrint)  Serial.println("NB-ERR: No output memory");
    AudioStream_F32::release(blockIn0);
    if(twoChannel)
       AudioStream_F32::release(blockIn1);
    return;
  }

  if(twoChannel)  {
     blockOut1 = AudioStream_F32::allocate_f32();
     if (!blockOut1) {      // Didn't have any
       if(errorPrint)  Serial.println("NB-ERR: No output 1 memory");
       AudioStream_F32::release(blockOut0);
       AudioStream_F32::release(blockIn0);
       AudioStream_F32::release(blockIn1);
       return;
     }
  }

  // delayData0[], and 1, always represents 256 points of I-F data.  It is pre-gate and includes noise pulses.
  // Go through new data, point i at a time, entering to delay line, looking
  // for noise pulses.  Then in same loop, move data to output buffer blockOut0->data
  // based on whether gate is open or not.
  for(i=0;  i<block_size;  i++) {
      float32_t datai0 = blockIn0->data[i];     // ith data
      delayData0[(i+in_index) & delayBufferMask] = datai0;  // Put ith data to circular delay buffer
      if(twoChannel)  {
         float32_t datai1 = blockIn1->data[i]; 
         delayData1[(i+in_index) & delayBufferMask] = datai1; 
         }

      // All control comes from the 0 input (not the 1 input)
      absSignal = fabsf(datai0);    // Rectified I-F
      runningSum += absSignal;  // Update by adding one rectified point
      runningSum -= delayData0[(i + in_index - RUNNING_SUM_SIZE) & delayBufferMask]; // & subtract one

      pulseTime++;     // This keeps track of leading and trailing delays of the gate pulse
      if (absSignal > (threshold * runningSum)) {  // A noise pulse event
          if (gateOn == true) {   // Signals are flowing, this is beginning of noise pulse event
              gateOn = false;
              pulseTime = -nAnticipation;
          }
          else  {    // gateOn==false, we are already in a noise pulse event
              if (pulseTime > 0)  {   // Waiting for pulse event to end
                  pulseTime = 0;        // Keep waiting
              }
          }
      }
      else {  // Noise pulse is below threshold
          if (gateOn == true)  {  // Signals are flowing normally
              pulseTime = -9999;
          }
          else  {         // gateOn==false, we are already in a noise pulse event
              if(pulseTime >= nDecay)  {  // End of a pulse event, turn gate on
                  gateOn = true;
                  pulseTime = -9999;
              }
          }
      }
      // Ready to enter I-F data to output, offset in time by "nAnticipation"
      if (pulseTime == -9999)  {
          blockOut0->data[i] = delayData0[(256 + i - nAnticipation) & delayBufferMask];
          if(twoChannel)
             blockOut1->data[i] = delayData1[(256 + i - nAnticipation) & delayBufferMask];
      }
      else  {      //  -nAnticipation < pulseTime < nDecay   i.e., blanked out
          blockOut0->data[i] = 0.0f;
          if(twoChannel)
             blockOut1->data[i] = 0.0f; 
      }
  }    // End of loop point by point over input 128 data points

  AudioStream_F32::release (blockIn0);
  AudioStream_F32::transmit(blockOut0, 0); // send the delayed or blanked data
  AudioStream_F32::release (blockOut0);
  if(twoChannel) {
     AudioStream_F32::release (blockIn1);
     AudioStream_F32::transmit(blockOut1, 1); // send second "Q" channel
     AudioStream_F32::release (blockOut1);
     }

  // Update pointer in_index to delay line for next 128 update
  in_index = (in_index + block_size) & delayBufferMask;
}

