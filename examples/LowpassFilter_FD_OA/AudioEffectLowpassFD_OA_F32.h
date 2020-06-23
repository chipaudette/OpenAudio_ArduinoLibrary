/*
 *  This include is NOT global to the OpenAudio library, but supports
 * LowpassFilter_FD.ino
 */
#ifndef _AudioEffectLowpassFD_F32_h
#define _AudioEffectLowpassFD_F32_h

#include "AudioStream_F32.h"
#include <arm_math.h>
#include "FFT_Overlapped_OA_F32.h"

class AudioEffectLowpassFD_F32 : public AudioStream_F32
{
  public:
    // constructors...a few different options.  The usual one should be:
    // AudioEffectLowpassFD_F32(const AudioSettings_F32 &settings, const int _N_FFT)
    AudioEffectLowpassFD_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {};
    AudioEffectLowpassFD_F32(const AudioSettings_F32 &settings) :
        AudioStream_F32(1, inputQueueArray_f32) {  sample_rate_Hz = settings.sample_rate_Hz;   }
    AudioEffectLowpassFD_F32(const AudioSettings_F32 &settings, const int _N_FFT) : 
        AudioStream_F32(1, inputQueueArray_f32) { setup(settings,_N_FFT);  }

    //destructor...release all of the memory that has been allocated
    ~AudioEffectLowpassFD_F32(void) {
      if (complex_2N_buffer != NULL) delete complex_2N_buffer;
    }
       
    int setup(const AudioSettings_F32 &settings, const int _N_FFT) {
      sample_rate_Hz = settings.sample_rate_Hz;
      int N_FFT;
      
      //setup the FFT and IFFT.  If they return a negative FFT, it wasn't an allowed FFT size.
      N_FFT = myFFT.setup(settings,_N_FFT); //hopefully, we got the same N_FFT that we asked for
      if (N_FFT < 1) return N_FFT;
      N_FFT = myIFFT.setup(settings,_N_FFT);  //hopefully, we got the same N_FFT that we asked for
      if (N_FFT < 1) return N_FFT;

      //decide windowing
      Serial.println("AudioEffectLowpassFD_F32: setting myFFT to use hanning...");
      (myFFT.getFFTObject())->useHanningWindow(); //applied prior to FFT
      //if (myIFFT.getNBuffBlocks() > 3) {
      //  Serial.println("AudioEffectLowpassFD_F32: setting myIFFT to use hanning...");
      //  (myIFFT.getIFFTObject())->useHanningWindow(); //window again after IFFT
      //}

      //print info about setup
      Serial.println("AudioEffectLowpassFD_F32: FFT parameters...");
      Serial.print("    : N_FFT = "); Serial.println(N_FFT);
      Serial.print("    : audio_block_samples = "); Serial.println(settings.audio_block_samples);
      Serial.print("    : FFT N_BUFF_BLOCKS = "); Serial.println(myFFT.getNBuffBlocks());
      Serial.print("    : IFFT N_BUFF_BLOCKS = "); Serial.println(myIFFT.getNBuffBlocks());
      Serial.print("    : FFT use window = "); Serial.println(myFFT.getFFTObject()->get_flagUseWindow());
      Serial.print("    : IFFT use window = "); Serial.println((myIFFT.getIFFTObject())->get_flagUseWindow());
      
      //allocate memory to hold frequency domain data
      complex_2N_buffer = new float32_t[2*N_FFT];

      //we're done.  return!
      enabled=1;
      return N_FFT;
    }

    void setLowpassFreq_Hz(float freq_Hz) { lowpass_freq_Hz = freq_Hz;  }
    
    float getLowpassFreq_Hz(void) {   return lowpass_freq_Hz; }
    
    virtual void update(void);

  private:
    int enabled=0;
    float32_t *complex_2N_buffer;
    audio_block_f32_t *inputQueueArray_f32[1];
    FFT_Overlapped_OA_F32 myFFT;
    IFFT_Overlapped_OA_F32 myIFFT;
    float lowpass_freq_Hz = 1000.f;
    float sample_rate_Hz = AUDIO_SAMPLE_RATE;   
};


void AudioEffectLowpassFD_F32::update(void)
{
  //get a pointer to the latest data
  audio_block_f32_t *in_audio_block = AudioStream_F32::receiveReadOnly_f32();
  if (!in_audio_block) return;

  //simply return the audio if this class hasn't been enabled
  if (!enabled) { AudioStream_F32::transmit(in_audio_block); AudioStream_F32::release(in_audio_block); return; }

  //convert to frequency domain
  myFFT.execute(in_audio_block, complex_2N_buffer);
  AudioStream_F32::release(in_audio_block);  //We just passed ownership to myFFT, so release it here.
  
  // ////////////// Do your processing here!!!

  //this is lowpass, so zero the bins above the cutoff
  int NFFT = myFFT.getNFFT();
  int nyquist_bin = NFFT/2 + 1;
  float bin_width_Hz = sample_rate_Hz / ((float)NFFT);
  int cutoff_bin = (int)(lowpass_freq_Hz / bin_width_Hz + 0.5); //the 0.5 is so that it rounds instead of truncates
  if (cutoff_bin < nyquist_bin) {
    for (int i=cutoff_bin; i < nyquist_bin; i++) { //only do positive frequency space...will rebuild the neg freq space later
      #if 0
        //zero out the bins (silence
        complex_2N_buffer[2*i] = 0.0f;  //real
        complex_2N_buffer[2*i+1]= 0.0f; //imaginary
     #else
        //attenuate by 30 dB
        complex_2N_buffer[2*i] *= 0.03f;  //real
        complex_2N_buffer[2*i+1] *= 0.03f; //imaginary
     #endif
    }
  }
  myFFT.rebuildNegativeFrequencySpace(complex_2N_buffer); //set the negative frequency space based on the positive

  // ///////////// End do your processing here

  //call the IFFT
  audio_block_f32_t *out_audio_block = myIFFT.execute(complex_2N_buffer); //out_block is pre-allocated in here.
 

  //send the returned audio block.  Don't issue the release command here because myIFFT will re-use it
  AudioStream_F32::transmit(out_audio_block); //don't release this buffer because myIFFT re-uses it within its own code
  return;
};
#endif
