#include <arm_math.h>
#include <AudioStream_F32.h>

class AudioMultiply_F32 : public AudioStream_F32
{
  public:
    AudioMultiply_F32(void) : AudioStream_F32(2, inputQueueArray_f32) {};
    void update(void);
    
  private:
    audio_block_f32_t *inputQueueArray_f32[2];
};
