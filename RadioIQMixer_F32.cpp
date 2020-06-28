/*
 * RadioIQMixer_F32.cpp
 *
 * 22 March 2020
 * Bob Larkin, in support of the library:
 * Chip Audette, OpenAudio, Apr 2017
 *     -------------------
 * A single signal channel comes in and is multiplied (mixed) with a sin
 * and cos of the same frequency.  The pair of mixer outputs are
 * referred to as i and q.  The conversion in frequency is either
 * up or down, and a pair of filters on i and q determine which is allow
 * to pass to the output. 
 * 
 * The sin/cos LO is from synth_sin_cos_f32.cpp  See that for details.
 * 
 * There are two then two outputs.
 * 
 * MIT License,  Use at your own risk.
*/

#include "RadioIQMixer_F32.h"
// 513 values of the sine wave in a float array:
#include "sinTable512_f32.h"

void RadioIQMixer_F32::update(void) {
  audio_block_f32_t *blockIn, *blockOut_i=NULL, *blockOut_q=NULL;
  uint16_t index, i;
  float32_t a, b, deltaPhase, phaseC;

  // Get first input, i, that will be filtered
  blockIn = AudioStream_F32::receiveWritable_f32(0);
  if (!blockIn) {
     if(errorPrintIQM)  Serial.println("IQMIXER-ERR: No input memory");
     return;
  }

  // Try to get a pair of blocks for the IQ output
  blockOut_i = AudioStream_F32::allocate_f32();
  if (!blockOut_i){      // Didn't have any  
    if(errorPrintIQM)  Serial.println("IQMIXER-ERR: No I output memory");
    AudioStream_F32::release(blockIn);
    return;
  }
  blockOut_q = AudioStream_F32::allocate_f32();
  if (!blockOut_q){
    if(errorPrintIQM)  Serial.println("IQMIXER-ERR: No Q output memory");
    AudioStream_F32::release(blockIn);
    AudioStream_F32::release(blockOut_i);
    return;
  }

    // doSimple has amplitude (-1, 1) and sin/cos differ by 90.00 degrees.
    if (doSimple) {
       for (i=0; i < block_size; i++) {
          phaseS += phaseIncrement;
          if (phaseS > 512.0f)
            phaseS -= 512.0f;
          index = (uint16_t) phaseS;
          deltaPhase = phaseS -(float32_t) index;
          /* Read two nearest values of input value from the sin table */
          a = sinTable512_f32[index];
          b = sinTable512_f32[index+1];
          // Linear interpolation and multiplying (DBMixer) with input
          blockOut_i->data[i] = blockIn->data[i] * (a + 0.001953125*(b-a)*deltaPhase);
 
          /* Repeat for cosine by adding 90 degrees phase  */
          index = (index + 128) & 0x01ff;
          /* Read two nearest values of input value from the sin table */
          a = sinTable512_f32[index];
          b = sinTable512_f32[index+1];
          /* deltaPhase will be the same as used for sin  */
          blockOut_q->data[i] = blockIn->data[i]*(a + 0.001953125*(b-a)*deltaPhase);
       }
    }
    else {   // Do a more flexible update, i.e., not doSimple
       for (i=0; i < block_size; i++) {
          phaseS += phaseIncrement;
          if (phaseS > 512.0f)  phaseS -= 512.0f;
          index = (uint16_t) phaseS;
          deltaPhase = phaseS -(float32_t) index;
          /* Read two nearest values of input value from the sin table */
          a = sinTable512_f32[index];
          b = sinTable512_f32[index+1];
          // We now have a sine value, so multiply with the input data and save
          // Linear interpolate sine and multiply with the input and amplitude (about 1.0)
          blockOut_i->data[i] = amplitude_pk * blockIn->data[i] * (a + 0.001953125*(b-a)*deltaPhase);
 
          /* Shift forward phaseS_C  and get cos. First, the calculation of index of the table */
          phaseC = phaseS + phaseS_C;
          if (phaseC > 512.0f)  phaseC -= 512.0f;
          index = (uint16_t) phaseC;
          deltaPhase = phaseC -(float32_t) index;
          /* Read two nearest values of input value from the sin table */
          a = sinTable512_f32[index];
          b = sinTable512_f32[index+1];
          // Same as sin, but leave amplitude of LO at +/- 1.0
          blockOut_q->data[i] = blockIn->data[i] * (a + 0.001953125*(b-a)*deltaPhase);
        }
    }
    AudioStream_F32::release(blockIn);   // Done with this
    //transmit the data
    AudioStream_F32::transmit(blockOut_i, 0); // send the I outputs
    AudioStream_F32::release(blockOut_i);
    AudioStream_F32::transmit(blockOut_q, 1);    // and the Q outputs
    AudioStream_F32::release(blockOut_q);
  }
