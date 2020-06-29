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
  audio_block_f32_t *blockIn, *blockOut=NULL;
  uint16_t i;
  float32_t absSignal;

  // Get input block   // <<Writable??
  blockIn = AudioStream_F32::receiveWritable_f32(0);
  if (!blockIn) {
     if(errorPrint)  Serial.println("NB-ERR: No input memory");
     return;
  }
  
  // Are we noise blanking?
  if(! runNB) {
      AudioStream_F32::transmit(blockIn, 0); // send the delayed or blanked data
      AudioStream_F32::release (blockIn);
      return;
  }

  // Get a block for the output
  blockOut = AudioStream_F32::allocate_f32();
  if (!blockOut){      // Didn't have any
    if(errorPrint)  Serial.println("NB-ERR: No output memory");
    AudioStream_F32::release(blockIn);
    return;
  }

  // delayData[] always represents 256 points of I-F data.  It is pre-gate and includes noise pulses.
  // Go through new data, point i at a time, entering to delay line, looking
  // for noise pulses.  Then in same loop, move data to output buffer blockOut->data
  // based on whether gate is open or not.
  for(i=0;  i<block_size;  i++) {
      float32_t datai = blockIn->data[i];     // ith data
      delayData[(i+in_index) & delayBufferMask] = datai;  // Put ith data to circular delay buffer

      absSignal = fabsf(datai);    // Rectified I-F
      runningSum += fabsf(datai);  // Update by adding one rectified point
      runningSum -= delayData[(i + in_index - RUNNING_SUM_SIZE) & delayBufferMask]; // & subtract one

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
      if (pulseTime == -9999)
          blockOut->data[i] = delayData[(256 + i - nAnticipation) & delayBufferMask];  // Need 256??
      else        //  -nAnticipation < pulseTime < nDecay   i.e., blanked out
          blockOut->data[i] = 0.0f;
  }    // End of loop point by point over input 128 data points

  AudioStream_F32::release (blockIn);
  AudioStream_F32::transmit(blockOut, 0); // send the delayed or blanked data
  AudioStream_F32::release (blockOut);

  // Update pointer in_index to delay line for next 128 update
  in_index = (in_index + block_size) & delayBufferMask;
}

