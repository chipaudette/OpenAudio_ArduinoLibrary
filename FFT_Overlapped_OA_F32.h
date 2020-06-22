/*
 * FFT_Overrlapped_OA_F32
 *
 * Purpose: Encapsulate the ARM floating point FFT/IFFT functions
 *          in a way that naturally interfaces to my float32
 *          extension of the Teensy Audio Library.
 *
 *          Provides functionality to do overlapped FFT/IFFT where
 *          each audio block is a fraction (1, 1/2, 1/4) of the
 *          totaly FFT length.  This class handles all of the
 *          data shuffling to composite the previous data blocks
 *          with the current data block to provide the full FFT.
 *          Does similar data shuffling (overlapp-add) for IFFT.
 *
 * Created: Chip Audette (openaudio.blogspot.com)
 *          Jan-Jul 2017
 *
 * Typical Usage as FFT:
 *            //setup the audio stuff
 *            float sample_rate_Hz = 44100.0;  //define sample rate
 *            int audio_block_samples = 32;   //define size of audio blocks
 *            AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);
 *            // ... continue creating all of your Audio Processing Blocks ...
 *
 *            // within a custom audio processing algorithm that you've written
 *            // you'd create the FFT and IFFT elements
 *            int NFFT = 128; //define length of FFT that you want (multiple of audio_block_samples)
 *            FFT_Overrlapped_F32 FFT_obj(audio_settings,NFFT); //Creare FFT object
 *            FFT_Overrlapped_F32 IFFT_obj(audio_settings,NFFT); //Creare IFFT object
 *            float complex_2N_buffer[2*NFFT];  //create buffer to hold the FFT output
 *
 *            // within your own algorithm's "update()" function (which is what
 *            // is called automatically by the Teensy Audio Libarary approach
 *            // to audio processing), you can execute the FFT and IFFT
 *
 *            // First, get the audio and convert to frequency-domain using an FFT
 *            audio_block_f32_t *in_audio_block = AudioStream_F32::receiveReadOnly_f32();
 *            FFT_obj.execute(in_audio_block, complex_2N_buffer); //output is in complex_2N_buffer
 *            AudioStream_F32::release(in_audio_block);  //We just passed ownership to FFT_obj, so release it here.
 *
 *            // Next do whatever processing you'd like on the frequency domain data
 *            // that is held in complex_2N_buffer
 *
 *            // Finally, you can convert back to the time domain via IFFT
 *            audio_block_f32_t *out_audio_block = IFFT_obj.execute(complex_2N_buffer);
 *            //note that the "out_audio_block" is mananged by IFFT_obj, so don't worry about releasing it.
 *
 *
 *  https://forum.pjrc.com/threads/53668-fft-ifft?highlight=IFFT  willie.from.texas 9-10-2018
 *  I've been using the CMSIS Version 5.3.0 DSP Library since May 2018 (see github.com/ARM-software/CMSIS_5).
 *  I might be the only person on this forum using it. The library allows me to use the 32-bit floating point
 *  capability of the Teensy 3.6. I have been able to use it in real-time with 16-bit sample rates
 *  out of both ADCs up to around 450 kHz (i-q data). I'm very happy with the performance I am obtaining.
 *  Here is the FFT/IFFT performance I am getting with the library:
 *       # Points   Forward rfft    Inverse rfft   Forward cfft  Inverse cfft
 *          512         201 us          247 us        239 us        294 us
 *         1024         362 us          454 us        588 us        714 us
 *         2048         846 us         1066 us       1376 us       1620 us
 *         4096        1860 us         2304 us       2504 us       2990 us
 *
 *
 * License: MIT License
 */
#ifndef _FFT_Overlapped_OA_F32_h
#define _FFT_Overlapped_OA_F32_h

#include "AudioStream_F32.h"
#include <arm_math.h>
#include "FFT_OA_F32.h"
//#include "utility/dspinst.h"  //copied from analyze_fft256.cpp.  Do we need this?

// set the max amount of allowed overlap...some number larger than you'll want to use
#define MAX_N_BUFF_BLOCKS 32  //32 blocks x 16 sample blocks enables NFFT = 512, if the Teensy could keep up.

class FFT_Overlapped_Base_OA_F32 {  //handles all the data structures for the overlapping stuff.  Doesn't care if FFT or IFFT
  public:
    FFT_Overlapped_Base_OA_F32(void) {};
    ~FFT_Overlapped_Base_OA_F32(void) {
      if (N_BUFF_BLOCKS > 0) {
        for (int i = 0; i < N_BUFF_BLOCKS; i++) {
          if (buff_blocks[i] != NULL) AudioStream_F32::release(buff_blocks[i]);
        }
      }
      if (complex_buffer != NULL) delete complex_buffer;
    }

    virtual int setup(const AudioSettings_F32 &settings, const int _N_FFT) {
      int N_FFT;

      ///choose valid _N_FFT
      if (!FFT_F32::is_valid_N_FFT(_N_FFT)) {
          Serial.println(F("FFT_Overlapped_Base_F32: *** ERROR ***"));
          Serial.print(F("  : N_FFT ")); Serial.print(_N_FFT);
          Serial.print(F(" is not allowed.  Try a power of 2 between 16 and 2048"));
          N_FFT = -1;
          return N_FFT;
      }

      //how many buffers will compose each FFT?
      audio_block_samples = settings.audio_block_samples;
      N_BUFF_BLOCKS = _N_FFT / audio_block_samples; //truncates!
      N_BUFF_BLOCKS = max(1,min(MAX_N_BUFF_BLOCKS,N_BUFF_BLOCKS));

      //what does the fft length actually end up being?
      N_FFT = N_BUFF_BLOCKS * audio_block_samples;

      //allocate memory for buffers...this is dynamic allocation.  Always dangerous.
      complex_buffer = new float32_t[2*N_FFT]; //should I check to see if it was successfully allcoated?

      //initialize the blocks for holding the previous data
      for (int i = 0; i < N_BUFF_BLOCKS; i++) {
        buff_blocks[i] = AudioStream_F32::allocate_f32();
        clear_audio_block(buff_blocks[i]);
      }

      return N_FFT;
    }
    virtual int getNFFT(void) = 0;
    virtual int getNBuffBlocks(void) { return N_BUFF_BLOCKS; }

  protected:
    int N_BUFF_BLOCKS = 0;
    int audio_block_samples;

    audio_block_f32_t *buff_blocks[MAX_N_BUFF_BLOCKS];
    float32_t *complex_buffer;

    void clear_audio_block(audio_block_f32_t *block) {
      for (int i = 0; i < block->length; i++) block->data[i] = 0.f;
    }
};

class FFT_Overlapped_OA_F32: public FFT_Overlapped_Base_OA_F32
{
  public:
    //constructors
    FFT_Overlapped_OA_F32(void): FFT_Overlapped_Base_OA_F32() {};
    FFT_Overlapped_OA_F32(const AudioSettings_F32 &settings): FFT_Overlapped_Base_OA_F32()  { }
    FFT_Overlapped_OA_F32(const AudioSettings_F32 &settings, const int _N_FFT): FFT_Overlapped_Base_OA_F32()  {
      setup(settings,_N_FFT);
    }

    virtual int setup(const AudioSettings_F32 &settings, const int _N_FFT) {
      int N_FFT = FFT_Overlapped_Base_OA_F32::setup(settings, _N_FFT);

      //setup the FFT routines
      N_FFT = myFFT.setup(N_FFT);
      return N_FFT;
    }

    virtual void execute(audio_block_f32_t *block, float *complex_2N_buffer);
    virtual int getNFFT(void) { return myFFT.getNFFT(); };
    FFT_F32* getFFTObject(void) { return &myFFT; };
    virtual void rebuildNegativeFrequencySpace(float *complex_2N_buffer) { myFFT.rebuildNegativeFrequencySpace(complex_2N_buffer); }

  private:
    FFT_F32 myFFT;
};

class IFFT_Overlapped_OA_F32: public FFT_Overlapped_Base_OA_F32
{
  public:
    //constructors
    IFFT_Overlapped_OA_F32(void): FFT_Overlapped_Base_OA_F32() {};
    IFFT_Overlapped_OA_F32(const AudioSettings_F32 &settings): FFT_Overlapped_Base_OA_F32()  { }
    IFFT_Overlapped_OA_F32(const AudioSettings_F32 &settings, const int _N_FFT): FFT_Overlapped_Base_OA_F32()  {
      setup(settings,_N_FFT);
    }

    virtual int setup(const AudioSettings_F32 &settings, const int _N_FFT) {
      int N_FFT = FFT_Overlapped_Base_OA_F32::setup(settings, _N_FFT);

      //setup the FFT routines
      N_FFT = myIFFT.setup(N_FFT);
      return N_FFT;
    }

    virtual audio_block_f32_t* execute(float *complex_2N_buffer);
    virtual int getNFFT(void) { return myIFFT.getNFFT(); };
    IFFT_F32* getFFTObject(void) { return &myIFFT; };
    IFFT_F32* getIFFTObject(void) { return &myIFFT; };
  private:
    IFFT_F32 myIFFT;
};
#endif
