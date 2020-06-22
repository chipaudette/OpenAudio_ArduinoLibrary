
#include "FFT_Overlapped_OA_F32.h"

void FFT_Overlapped_OA_F32::execute(audio_block_f32_t *block, float *complex_2N_buffer) //results returned inc omplex_2N_buffer
{
  int targ_ind;

  //get a pointer to the latest data
  //audio_block_f32_t *block = AudioStream_F32::receiveReadOnly_f32();
  if (!block) return;

  //add a claim to this block.  As a result, be sure that this function issues a "release()".
  //Also, be sure that the calling function issues its own release() to release its claim.
  block->ref_count++;   
  
  //shuffle all of input data blocks in preperation for this latest processing
  AudioStream_F32::release(buff_blocks[0]);  //release the oldest one...this is the release the corresponds to the claim above
  for (int i = 1; i < N_BUFF_BLOCKS; i++) buff_blocks[i - 1] = buff_blocks[i];
  buff_blocks[N_BUFF_BLOCKS - 1] = block; //append the newest input data to the complex_buffer blocks

  //copy all input data blocks into one big block...the big block is interleaved [real,imaginary]
  targ_ind = 0;
  //Serial.print("Overlapped_FFT_F32: N_BUFF_BLOCKS = "); Serial.print(N_BUFF_BLOCKS);
  //Serial.print(", audio_block_samples = "); Serial.println(audio_block_samples);
  for (int i = 0; i < N_BUFF_BLOCKS; i++) {
    for (int j = 0; j < audio_block_samples; j++) {
      complex_2N_buffer[2*targ_ind] = buff_blocks[i]->data[j];  //real
      complex_2N_buffer[2*targ_ind+1] = 0;  //imaginary
      targ_ind++;
    }
  }
  //call the FFT...windowing of the data happens in the FFT routine, if configured
  myFFT.execute(complex_2N_buffer);
}

audio_block_f32_t* IFFT_Overlapped_OA_F32::execute(float *complex_2N_buffer) { //real results returned through audio_block_f32_t

  //Serial.print("Overlapped_IFFT_F32: N_BUFF_BLOCKS = "); Serial.print(N_BUFF_BLOCKS);
  //Serial.print(", audio_block_samples = "); Serial.println(audio_block_samples);
  

  //call the IFFT...any follow-up windowing is handdled in the IFFT routine, if configured
  myIFFT.execute(complex_2N_buffer);
  
  
  //prepare for the overlap-and-add for the output
  audio_block_f32_t *temp_buff = buff_blocks[0]; //hold onto this one for a moment...it'll get overwritten later
  for (int i = 1; i < N_BUFF_BLOCKS; i++) buff_blocks[i - 1] = buff_blocks[i]; //shuffle the output data blocks
  buff_blocks[N_BUFF_BLOCKS - 1] = temp_buff; //put the oldest output buffer back in the list

  //do overlap and add with previously computed data
  int output_count = 0;
  for (int i = 0; i < (N_BUFF_BLOCKS-1); i++) { //Notice that this loop does NOT do the last block.  That's a special case after.
    for (int j = 0; j < audio_block_samples; j++) {
      buff_blocks[i]->data[j] +=  complex_2N_buffer[2*output_count]; //add only the real part into the previous results
      output_count++;
    }
  }

  //now write in the newest data into the last block, overwriting any garbage that might have existed there
  for (int j = 0; j < audio_block_samples; j++) {
    buff_blocks[N_BUFF_BLOCKS - 1]->data[j] =  complex_2N_buffer[2*output_count]; //overwrite with the newest data
    output_count++;
  }

  //send the oldest data.  Don't issue the release command here because we will release it the next time through this routine
  //transmit(buff_blocks[0]); //don't release this buffer because we re-use it every time this is called
  return buff_blocks[0]; //send back the pointer to this audio block...but don't release it because we'll re-use it here
};

