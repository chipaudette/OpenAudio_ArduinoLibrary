#ifndef _AudioMerge_I16toI32_h
#define _AudioMerge_I16toI32_h

#include <AudioStream_F32.h>

// class AudioMerge_I16toI32 : public AudioStream_F32 //receive Int and transmits Float
class AudioMerge_I16toI32 : public AudioStream //receive Int and transmits Float
{
  //GUI: inputs:2, outputs:1  //this line used for automatic generation of GUI node
  public:
    // AudioMerge_I16toI32(void) : AudioStream_F32(2, inputQueueArray_f32) { };  // specify two inputs here
	  // AudioMerge_I16toI32(const AudioSettings_F32 &settings) : AudioStream_F32(2, inputQueueArray_f32) { };  // specify 2 inputs here
  
    AudioMerge_I16toI32(void) : AudioStream(2, inputQueueArray) { };  // specify two inputs here
	  AudioMerge_I16toI32(const AudioSettings_F32 &settings) : AudioStream(2, inputQueueArray) { };  // specify 2 inputs here


    void update(void) {	
      //get the Int16 blocks
      audio_block_t *int_block_1;
      audio_block_t *int_block_2;
      int_block_1 = AudioStream::receiveReadOnly(0);  // first int16 data block
      int_block_2 = AudioStream::receiveReadOnly(1);  // second int16 data block
      if (int_block_1 == NULL) return;
      if (int_block_2 == NULL) {
        AudioStream::release(int_block_1);
        return;
      }

      // Allocate a single float block
      audio_block_f32_t *float_block;
      float_block = AudioStream_F32::allocate_f32(); 
      if (float_block == NULL) {
        AudioStream::release(int_block_1);
        AudioStream::release(int_block_2);
        return;
      }
      
      // Merge two I16 streams into a single F32 stream
      mergeAudio_I16toF32(int_block_1, int_block_2, float_block, float_block->length);

      //transmit the audio and return it to the system
      AudioStream_F32::transmit(float_block, 0);
      AudioStream_F32::release(float_block);
      AudioStream::transmit(int_block_1);
      AudioStream::transmit(int_block_2);
      AudioStream::release(int_block_1);
      AudioStream::release(int_block_2);
    };
    
    static void mergeAudio_I16toF32(audio_block_t *in_1, audio_block_t *in_2, audio_block_f32_t *out, int len) {
      // const float MAX_INT_16 = 32768.0;
      // const float MAX_INT_32 = 2147483648.0;

      for (int i = 0; i < len; i++) {
        // Convert to unsigned
        uint16_t u1 = (uint16_t)in_1->data[i];
        uint16_t u2 = (uint16_t)in_2->data[i];
        uint32_t u_combined = (u1 << 16) | u2;
        out->data[i] = (float)u_combined;  // this will have rounding errors
      }
      arm_scale_f32(out->data, 1./2147483648., out->data, out->length);  // this puts it between 0 and 2.0
      arm_offset_f32(out->data, -1., out->data, out->length);  // this scales it to -1.0 and +1.0
    }    
    
  private:
    audio_block_f32_t *inputQueueArray_f32[2];
    audio_block_t *inputQueueArray[2];

};


#endif