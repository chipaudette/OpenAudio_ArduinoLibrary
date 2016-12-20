
/*
   AudioEffectMine

   Created: Chip Audette, December 2016
   Purpose; Here is the skeleton of a audio processing algorithm that will
       (hopefully) make it easier for people to start making their own 
       algorithm.
  
   This processes a single stream fo audio data (ie, it is mono)

   MIT License.  use at your own risk.
*/

#include <arm_math.h> //ARM DSP extensions.  https://www.keil.com/pack/doc/CMSIS/DSP/html/index.html
#include <AudioStream_F32.h>

class AudioEffectMine_F32 : public AudioStream_F32
{
   public:
    //constructor
    AudioEffectMine_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {
      //do any setup activities here
    };

    //here's the method that is called automatically by the Teensy Audio Library
    void update(void) {
      //Serial.println("AudioEffectMine_F32: doing update()");  //for debugging.
      audio_block_f32_t *audio_block;
      audio_block = AudioStream_F32::receiveWritable_f32();
      if (!audio_block) return;

      //do your work
      applyMyAlgorithm(audio_block);

      ///transmit the block and release memory
      AudioStream_F32::transmit(audio_block);
      AudioStream_F32::release(audio_block);
    }

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!11
    // Here is where you can add your algorithm.
    // This function gets called block-wise...which is usually hard-coded to every 128 samples
    void applyMyAlgorithm(audio_block_f32_t *audio_block) {
      
      //Add whatever you'd like here.  I'm going to show a couple of examples below.
      //you can delete all of these examples.  They're just to illustrate.
      //More examples of fast (DSP accelerated) operations are at: https://github.com/chipaudette/OpenAudio/blob/master/Docs/Programming%20Algorithms/Using%20DSP%20Exentions.md

      // //apply a fixed gain...the whole block is processed with this one command
      //float32_t gain = 2.0;  %here's 6 dB of gain
      //arm_scale_f32(audio_block->data, gain, audio_block->data, audio_block->length); //uses ARM DSP for speed!

      // //compute the power of each sample...the whole block is processed with this one command
      // audio_block_f32_t *audio_pow_block = AudioStream_F32::allocate_f32();
      // arm_mult_f32(audio_block->data, audio_block->data, audio_pow_block->data, audio_block->length);

      // //loop over each sample and do something on a point-by-point basis (when it cannot be done as a block)
      //for (int i=0; i < audio_block->length; i++) {
      //  //as a boring example, let's add the user_parameter value to every sample.  yes, this
      //  //addition operation could have been done with a DSP-accelerated function, but I'm 
      //  //simply trying to illustrate how to loop on your audio samples.
      //  audio_block->data[i] = audio_block->data[i] + user_parameter;  
      //}

      // //clean up...if you allocated any memory in the lines above
      //Audiostream_F32::release(audio_pow_block);
      
      // return your processed audio by putting it back into audio_block->data
      
    } //end of applyMyAlgorithms
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


    // Call this method in your main program if you want to set the value of your user parameter. 
    // The user parameter can be used in your algorithm above.  The user_parameter variable was
    // created in the "private:" section of this class, which appears a little later in this file.
    // Feel free to create more user parameters (and to use better names for your variables)
    // for use in this class.
    float32_t setUserParameter(float val) {
      return user_parameter = val;
    }
 
  private:
    //state-related variables
    audio_block_f32_t *inputQueueArray_f32[1]; //memory pointer for the input to this module

    //this value can be set from the outside (such as from the potentiometer) to control
    //a parameter within your algorithm
    float32_t user_parameter = 0.0;   

};  //end class definition for AudioEffectMine_F32

