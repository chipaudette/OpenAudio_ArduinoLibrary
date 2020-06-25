/*
 * AudioEffectFreqShiftFD_F32
 * 
 * Created: Chip Audette, Aug 2019
 * Purpose: Shift the frequency content of the audio up or down.  Performed in the frequency domain
 *          
 * This processes a single stream of audio data (ie, it is mono)       
 *          
 * MIT License.  use at your own risk.
*/

#ifndef _AudioEffectFreqShiftFD_OA_F32_h
#define _AudioEffectFreqShiftFD_OA_F32_h

#include "AudioStream_F32.h"
#include <arm_math.h>
#include "FFT_Overlapped_OA_F32.h"
#include <Arduino.h>


class AudioEffectFreqShiftFD_OA_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:freq_shift
  public:
    //constructors...a few different options.  The usual one should be: AudioEffectFreqShiftFD_F32(const AudioSettings_F32 &settings, const int _N_FFT)
    AudioEffectFreqShiftFD_OA_F32(void) : AudioStream_F32(1, inputQueueArray_f32) {};
    AudioEffectFreqShiftFD_OA_F32(const AudioSettings_F32 &settings) :
      AudioStream_F32(1, inputQueueArray_f32) {
      sample_rate_Hz = settings.sample_rate_Hz;
    }
    AudioEffectFreqShiftFD_OA_F32(const AudioSettings_F32 &settings, const int _N_FFT) :
      AudioStream_F32(1, inputQueueArray_f32) {
      setup(settings, _N_FFT);
    }

    //destructor...release all of the memory that has been allocated
    ~AudioEffectFreqShiftFD_OA_F32(void) {
      if (complex_2N_buffer != NULL) delete complex_2N_buffer;
    }

    int setup(const AudioSettings_F32 &settings, const int _N_FFT) {
      sample_rate_Hz = settings.sample_rate_Hz;

      //setup the FFT and IFFT.  If they return a negative FFT, it wasn't an allowed FFT size.
      N_FFT = myFFT.setup(settings, _N_FFT); //hopefully, we got the same N_FFT that we asked for
      if (N_FFT < 1) return N_FFT;
      N_FFT = myIFFT.setup(settings, _N_FFT); //hopefully, we got the same N_FFT that we asked for
      if (N_FFT < 1) return N_FFT;
	  

      //decide windowing
      //Serial.println("AudioEffectFreqShiftFD_F32: setting myFFT to use hanning...");
      (myFFT.getFFTObject())->useHanningWindow(); //applied prior to FFT
      #if 1
        if (myIFFT.getNBuffBlocks() > 3) {
          Serial.println("AudioEffectFormantShiftFD_F32: setting myIFFT to use hanning...");
          (myIFFT.getIFFTObject())->useHanningWindow(); //window again after IFFT
        }
      #endif

 	  //decide how much overlap is happening
	  switch (myIFFT.getNBuffBlocks()) {
		  case 0:
			//should never happen
			break;
		  case 1:
		    overlap_amount = NONE;
			break;
		  case 2:
		    overlap_amount = HALF;
			break;
		  case 3:
			//to do...need to add phase shifting logic to the update() function to support this case
			break;
		  case 4:
			overlap_amount = THREE_QUARTERS;
		    //to do...need to add phase shifting logic to the update() function to support this case
			break;
	  }
			
	  
	  #if 0
      //print info about setup
      Serial.println("AudioEffectFreqShiftFD_F32: FFT parameters...");
      Serial.print("    : N_FFT = "); Serial.println(N_FFT);
      Serial.print("    : audio_block_samples = "); Serial.println(settings.audio_block_samples);
      Serial.print("    : FFT N_BUFF_BLOCKS = "); Serial.println(myFFT.getNBuffBlocks());
      Serial.print("    : IFFT N_BUFF_BLOCKS = "); Serial.println(myIFFT.getNBuffBlocks());
      Serial.print("    : FFT use window = "); Serial.println(myFFT.getFFTObject()->get_flagUseWindow());
      Serial.print("    : IFFT use window = "); Serial.println((myIFFT.getIFFTObject())->get_flagUseWindow());
	  #endif
	  
      //allocate memory to hold frequency domain data
      complex_2N_buffer = new float32_t[2 * N_FFT];

      //we're done.  return!
      enabled = 1;
      return N_FFT;
    }

    int setShift_bins(int _shift_bins) {
      return shift_bins = _shift_bins;
    }
    int getShift_bins(void) {
      return shift_bins;
    }
	float getShift_Hz(void) {
		return getFrequencyOfBin(shift_bins);
	}
	float getFrequencyOfBin(int bin) { //"bin" should be zero to (N_FFT-1)
		return sample_rate_Hz * ((float)bin) / ((float) N_FFT);
	}
	
    virtual void update(void);
	bool enable(bool state = true) { enabled = state; return enabled;}

  private:
    int enabled = 0;
    float32_t *complex_2N_buffer;
    audio_block_f32_t *inputQueueArray_f32[1];
    FFT_Overlapped_OA_F32 myFFT;
    IFFT_Overlapped_OA_F32 myIFFT;
    float sample_rate_Hz = AUDIO_SAMPLE_RATE;
	int N_FFT = -1;
	enum OVERLAP_OPTIONS {NONE, HALF, THREE_QUARTERS};  //evenutally extend to THREE_QUARTERS
	int overlap_amount = NONE;
	int overlap_block_counter = 0;
	
    int shift_bins = 0; //how much to shift the frequency
};


#endif
