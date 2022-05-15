
#ifndef _AudioConvert_I16toF32_h
#define _AudioConvert_I16toF32_h

#include <AudioStream_F32.h>

class AudioConvert_I16toF32 : public AudioStream_F32 //receive Int and transmits Float
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  public:
    AudioConvert_I16toF32(void) : AudioStream_F32(1, inputQueueArray_f32) { };
	AudioConvert_I16toF32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) { };
	
    void update(void) {	
      //get the Int16 block
      audio_block_t *int_block;
      int_block = AudioStream::receiveReadOnly(); //int16 data block
      if (int_block==NULL) return;

      //allocate a float block
      audio_block_f32_t *float_block;
      float_block = AudioStream_F32::allocate_f32(); 
      if (float_block == NULL) {
      	  AudioStream::release(int_block);
      	  return;
      }
      
      //convert to float
      convertAudio_I16toF32(int_block, float_block, float_block->length);

      //transmit the audio and return it to the system
      AudioStream_F32::transmit(float_block,0);
      AudioStream_F32::release(float_block);
      AudioStream::release(int_block);
    };
    
    static void convertAudio_I16toF32(audio_block_t *in, audio_block_f32_t *out, int len) {
      //WEA Method.  Should look at CMSIS arm_q15_to_float instead: https://www.keil.com/pack/doc/CMSIS/DSP/html/group__q15__to__x.html#gaf8b0d2324de273fc430b0e61ad4e9eb2
      const float MAX_INT = 32768.0;
      for (int i = 0; i < len; i++) out->data[i] = (float)(in->data[i]);
      arm_scale_f32(out->data, 1.0/MAX_INT, out->data, out->length); //divide by 32678 to get -1.0 to +1.0
    }    
    
  private:
    audio_block_f32_t *inputQueueArray_f32[1]; //2 for stereo

};


class AudioConvert_F32toI16 : public AudioStream_F32 //receive Float and transmits Int
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
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
      if (int_block == NULL) {
      	  AudioStream_F32::release(float_block);
      	  return;
      }
      
      //convert back to int16
      convertAudio_F32toI16(float_block, int_block, float_block->length);

      //return audio to the system
      AudioStream::transmit(int_block);
      AudioStream::release(int_block);
      AudioStream_F32::release(float_block);
    };

   static void convertAudio_F32toI16(audio_block_f32_t *in, audio_block_t *out, int len) {
      //WEA Method.  Should look at CMSIS arm_float_to_q15 instead: https://www.keil.com/pack/doc/CMSIS/DSP/html/group__float__to__x.html#ga215456e35a18db86882e1d3f0d24e1f2	
      const float MAX_INT = 32678.0;
      for (int i = 0; i < len; i++) {
        out->data[i] = (int16_t)(max(min( (in->data[i] * MAX_INT), MAX_INT), -MAX_INT));
      }
    }    
    
  private:
    audio_block_f32_t *inputQueueArray_Float[1];
 
};

#endif