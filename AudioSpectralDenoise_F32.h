/*
 * AudioSpectralDenoise_F32
 * 
 * Created: Graham Whaley, 2022
 * Purpose: Spectral noise reduction
 *          
 * This processes a single stream of audio data (i.e., it is mono)     
 *          
 * License: GNU GPLv3 License
 *  As the code it is derived from is GPLv3
 *
 * Based off the work from the UHSDR project, as also used in the mcHF and Convolution-SDR
 * projects.
 * Reference documentation can be found at https://github.com/df8oe/UHSDR/wiki/Noise-reduction
 * Code extracted into isolated files can be found at
 *   https://github.com/grahamwhaley/DSPham/blob/master/spectral.cpp
 */

#ifndef _AudioSpectralDenoise_F32_h
#define _AudioSpectralDenoise_F32_h

#include "AudioStream_F32.h"
#include <arm_math.h>
#include "FFT_Overlapped_OA_F32.h"
#include <Arduino.h>

class AudioSpectralDenoise_F32:public AudioStream_F32 {
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:spectral
public:
  AudioSpectralDenoise_F32(void):AudioStream_F32(1, inputQueueArray_f32) {
  };
  AudioSpectralDenoise_F32(const AudioSettings_F32 &
                           settings):AudioStream_F32(1, inputQueueArray_f32) {
  }
  AudioSpectralDenoise_F32(const AudioSettings_F32 & settings,
                           const int _N_FFT):AudioStream_F32(1,
                                                             inputQueueArray_f32)
  {
    setup(settings, _N_FFT);
  }

  //destructor...release all of the memory that has been allocated
  ~AudioSpectralDenoise_F32(void) {
    if (complex_2N_buffer) delete complex_2N_buffer;
    if (NR_X) delete NR_X;
    if (ph1y) delete ph1y;
    if (pslp) delete pslp;
    if (xt) delete xt;
    if (NR_SNR_post) delete NR_SNR_post;
    if (NR_SNR_prio) delete NR_SNR_prio;
    if (NR_Hk_old) delete NR_Hk_old;
    if (NR_G) delete NR_G;
    if (NR_Nest) delete NR_Nest;
  }

  //Our default FFT size is 256. That is time and space efficient, but
  // if you are running at a 'high' sample rate, the NR 'buckets' might
  // be quite small. You may want to use a 1024 FFT if running at 44.1KHz
  // for instance, if you can afford the time and space overheads.
  int setup(const AudioSettings_F32 & settings, const int _N_FFT = 256);

  virtual void update(void);
  bool enable(bool state = true) {
    is_enabled = state;
    return is_enabled;
  }
  bool enabled(void) {
    return is_enabled;
  }

  //Getters and Setters
  float32_t getAsnr(void) {
    return asnr;
  }
  void setAsnr(float32_t v) {
    asnr = v;
  }
  float32_t getVADHighFreq(void) {
    return VAD_high_freq;
  }
  void setVADHighFreq(float32_t f) {
    VAD_high_freq = f;
  }
  float32_t getVADLowFreq(void) {
    return VAD_low_freq;
  }
  void setVADLowFreq(float32_t f) {
    VAD_low_freq = f;
  }
  float32_t getNRAlpha(void) {
    return NR_alpha;
  }
  void setNRAlpha(float32_t v) {
    NR_alpha = v;
    if (NR_alpha < 0.9)
      NR_alpha = 0.9;
    if (NR_alpha > 0.9999)
      NR_alpha = 0.9999;
  }
  float32_t getSNRPrioMin(void) {
    return snr_prio_min;
  }
  void setSNRPrioMin(float32_t v) {
    snr_prio_min = v;
  }
  int16_t getNRWidth(void) {
    return NR_width;
  }
  void setNRWidth(int16_t v) {
    NR_width = v;
  }
  float32_t getPowerThreshold(void) {
    return power_threshold;
  }
  void setPowerThreshold(float32_t v) {
    power_threshold = v;
  }
  float32_t getTaxFactor(void) {
    return tax_factor;
  }
  void setTaxFactor(float32_t v) {
    tax_factor = v;
  }
  float32_t getTapFactor(void) {
    return tap_factor;
  }
  void setTapFactor(float32_t v) {
    tap_factor = v;
  }

private:
  static const int max_fft = 2048;  //The largest FFT FFT_OA handles. Fixed so we can fix the
  //array sizes - FIXME - a hack, but easier than doing the dynamic allocations for now.

  uint8_t init_phase = 1;       //Track our phases of initialisation
  int is_enabled = 0;
  float32_t *complex_2N_buffer; //Store our FFT real/imag data
  audio_block_f32_t *inputQueueArray_f32[1];  //memory pointer for the input to this module
  FFT_Overlapped_OA_F32 myFFT;
  IFFT_Overlapped_OA_F32 myIFFT;
  int N_FFT = -1;               //How big an FFT are we using?
  int N_bins = -1;              //How many actual data bins are we processing on
  float sample_rate_Hz = AUDIO_SAMPLE_RATE;

  //*********** NR vars
  //Magnitudes (fabs) of power for the last four (three?) audio blocks
  float32_t *NR_X = NULL;

  float32_t *ph1y = NULL;
  float32_t *pslp = NULL;
  float32_t *xt = NULL;

  const float32_t psini = 0.5;  //initial speech probability
  const float32_t pspri = 0.5;  //prior speech probability
  float32_t asnr = 25;          //active SNR in dB - seems to make less different than I expected.
  float32_t xih1;
  float32_t pfac;
  float32_t xih1r;

  const float32_t psthr = 0.99; //threshold for smoothed speech probability
  const float32_t pnsaf = 0.01; //noise probability safety value
  float32_t tinc;               //Frame time in seconds
  float32_t tax_factor = 0.8;	//Noise output smoothing factor
  float32_t tax;                //noise output smoothing constant in seconds = -tinc/ln(0.8)
  float32_t tap_factor = 0.9;	//Speech probability smoothing factor
  float32_t tap;                //speech prob smoothing constant in seconds = -tinc/ln(0.9)
  float32_t ap;                 //noise output smoothing factor
  float32_t ax;                 //noise output smoothing factor
  float32_t snr_prio_min = powf(10, -(float32_t) 20 / 20.0);  //Lower limit of SNR ratio calculation
  // Time smoothing of gain weights. Makes quite a difference to the NR performance.
  float32_t NR_alpha = 0.99;    //range 0.98-0.9999. 0.95 acts much too hard: reverb effects.

  float32_t *NR_SNR_post = NULL;
  float32_t *NR_SNR_prio = NULL;
  float32_t *NR_Hk_old = NULL;

  // preliminary gain factors (before time smoothing) and after that contains the frequency
  // smoothed gain factors
  float32_t *NR_G = NULL;

  //Our Noise estimate array - 'one dimentional' is a hangover from the old version of the
  // original code that used multiple entries for averaging, which seems to have then been
  // dropped, but the arrays still left in place.
  float32_t *NR_Nest = NULL;

  float32_t VAD_low_freq = 100.0;
  float32_t VAD_high_freq = 3600.0;
  //if we grow the FFT to 1024, these might need to be bigger than a uint8?
  uint8_t VAD_low, VAD_high;    //lower/upper bounds for 'voice spectrum' slot processing
  int16_t NN;                   //used as part of VAD calculations, n-bin averaging?. Also, why an int16 ?
  int16_t NR_width = 4;
  float32_t pre_power, post_power;  //Used in VAD calculations
  float32_t power_ratio;
  float32_t power_threshold = 0.4;
};

#endif
