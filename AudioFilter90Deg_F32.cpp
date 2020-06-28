/*
 * AudioFilter90Deg_F32.cpp
 *
 * 22 March 2020
 * Bob Larkin, in support of the library:
 * Chip Audette, OpenAudio, Apr 2017
 *     -------------------
 * There are two channels that update synchronously, but operate
 * independently.  The I-channel, coresponding to a "Left"
 * audio channel, is filtered with an N coefficient
 * Hilbert Transform  in FIR form.
 * The Q-channel, or "Right" channel, is simply
 * delayed by the (N-1)/2  sample periods.   The phase between
 * the two  outputs is 90-Degrees.  The amplitude response cuts off low
 * frequencies and depends on N.
 *
 * The I-channel FIR is a Hilbert Transform and this has every other term 0.
 * These need not be multiplied-and-accumulated, but for now, we do.  By the
 * time the indexes are calculated, it may be quicker to process everything.
 * Additionally, to not require a half-sample delay, the number of terms in
 * the Hilbert FIR needs to be odd.
 * 
 * MIT License,  Use at your own risk.
*/

#include "AudioFilter90Deg_F32.h"

void AudioFilter90Deg_F32::update(void)
{
  audio_block_f32_t *block_i, *block_q,  *blockOut_i=NULL;
  uint16_t i;

  // Get first input, i, that will be filtered
  block_i = AudioStream_F32::receiveWritable_f32(0);
  if (!block_i) {
	 if(errorPrint)  Serial.println("FIL90-ERR: No i input memory");
	 return;
  }

  // Get second input, q, that will be delayed
  block_q = AudioStream_F32::receiveWritable_f32(1);
  if (!block_q){
	if(errorPrint)  Serial.println("FIL90-ERR: No q input memory");
    AudioStream_F32::release(block_i);
   return;
  }

  // If there's no coefficient table, give up.
  if (coeff_p == NULL) {
	if(errorPrint)  Serial.println("FIL90-ERR: No Hilbert FIR coefficients");
    AudioStream_F32::release(block_i);
    AudioStream_F32::release(block_q);
    return;
  }
  
  // Try to get a block for the FIR output
  blockOut_i = AudioStream_F32::allocate_f32();
  if (!blockOut_i){      // Didn't have any
    if(errorPrint)  Serial.println("FIL90-ERR: No out 0 block");
    AudioStream_F32::release(block_i);  
    AudioStream_F32::release(block_q);
    return;
  }

  // Apply the Hilbert transform FIR.  This includes multiplies by zero.
  // Is there any way to play with the indexes and not multiply by zero
  // without spending more time than is saved? How about something like
  //           pick right nn,  then   for(j=0; j<fir_length; j+=2)   {sum += coef[j] * data[j+nn];}
  arm_fir_f32(&Ph90Deg_inst, block_i->data, blockOut_i->data, block_i->length);
  AudioStream_F32::release(block_i);     // Not needed further
  
  // Now enter block_size points to the delay loop and move earlier points to re-used block
  float32_t *pin, *pout;
  pin = &(block_q->data[0]);        //  point to beginning of block_q
  for(i=0;  i<block_size;  i++) {   // Let it wrap around at 256
	in_index++;
    in_index = delayBufferMask & in_index;     // Index into delay buffer, wrapping
    delayData[in_index] = *pin++;  // Put ith q data to circular delay buffer
  }
 
  // And similarly, output with a delay, i.e., a buffer offset
  pout = &(block_q->data[0]);       // Re-use the input block
  for(i=0;  i<block_size;  i++) {
	out_index++;
    out_index = delayBufferMask & out_index;     // Index into delay buffer, wrapping
    *pout++ = delayData[out_index];
  }

  //transmit the data
  AudioStream_F32::transmit(blockOut_i, 0); // send the FIR outputs
  AudioStream_F32::release (blockOut_i);
  AudioStream_F32::transmit(block_q, 1);    // and the delayed outputs
  AudioStream_F32::release (block_q);
}

