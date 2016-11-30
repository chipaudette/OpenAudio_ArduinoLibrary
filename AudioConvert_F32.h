
#ifndef _AudioConvert_I16toF32
#define _AudioConvert_I16toF32


#include <AudioStream_F32.h>

class AudioConvert_I16toF32 : public AudioStream_F32 //receive Int and transmits Float
{
  public:
    AudioConvert_I16toF32(void) : AudioStream_F32(1, inputQueueArray_f32) { };
    void update(void) {
      //get the Int16 block
      audio_block_t *int_block;
      int_block = AudioStream::receiveReadOnly(); //int16 data block
      if (!int_block) return;

      //allocate a float block
      audio_block_f32_t *float_block;
      float_block = AudioStream_F32::allocate_f32(); 
      if (float_block == NULL) return;
      
      //convert to float
      convertAudio_I16toF32(int_block, float_block, AUDIO_BLOCK_SAMPLES);

      //transmit the audio and return it to the system
      AudioStream_F32::transmit(float_block,0);
      AudioStream_F32::release(float_block);
      AudioStream::release(int_block);
    };
    
  private:
    audio_block_f32_t *inputQueueArray_f32[1]; //2 for stereo

    static void convertAudio_I16toF32(audio_block_t *in, audio_block_f32_t *out, int len) {
      const float MAX_INT = 32678.0;
      //for (int i = 0; i < len; i++) out->data[i] = (float)(in->data[i])/MAX_INT;
      for (int i = 0; i < len; i++) out->data[i] = (float)(in->data[i]);
      arm_scale_f32(out->data, 1.0/MAX_INT, out->data, out->length); //divide by 32678 to get -1.0 to +1.0
    }
};


class AudioConvert_F32toI16 : public AudioStream_F32 //receive Float and transmits Int
{
  public:
    AudioConvert_F32toI16(void) : AudioStream_F32(1, inputQueueArray_Float) {};
    void update(void) {
      //get the float block
      audio_block_f32_t *float_block;
      float_block = AudioStream_F32::receiveReadOnly_f32(); //float data block
      if (!float_block) return;

      //allocate a Int16 block
      audio_block_t *int_block;
      int_block = AudioStream::allocate(); 
      if (int_block == NULL) return;
      
      //convert back to int16
      convertAudio_F32ToI16(float_block, int_block, AUDIO_BLOCK_SAMPLES);

      //return audio to the system
      AudioStream::transmit(int_block);
      AudioStream::release(int_block);
      AudioStream_F32::release(float_block);
    };
    
  private:
    audio_block_f32_t *inputQueueArray_Float[1];
    static void convertAudio_F32ToI16(audio_block_f32_t *in, audio_block_t *out, int len) {
      const float MAX_INT = 32678.0;
      for (int i = 0; i < len; i++) {
        out->data[i] = (int16_t)(max(min( (in->data[i] * MAX_INT), MAX_INT), -MAX_INT));
      }
    }
};

#endif