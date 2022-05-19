/* AudioSpectralDenoise_F2.h
 * Spectral noise reduction
 *
 * Extracted and based on the work found in the:
 *  - Convolution SDR:  https://github.com/DD4WH/Teensy-ConvolutionSDR
 *  - UHSDR: https://github.com/df8oe/UHSDR/blob/active-devel/mchf-eclipse/drivers/audio/audio_nr.c
 *
 * License: GNU GPLv3
 *  Both the Convolution SDR and UHSDR are licensed under GPLv3.
 */

#include "AudioSpectralDenoise_F32.h"

#include <new>

// No serial debug by default
static const bool serial_debug = false;

int AudioSpectralDenoise_F32::setup(const AudioSettings_F32 & settings,
                                    const int _N_FFT)
{
  enable(false);                //Disable us, just incase we are already active...
  sample_rate_Hz = settings.sample_rate_Hz;

  if (N_FFT == -1) {
    //setup the FFT and IFFT.  If they return a negative FFT, it wasn't an allowed FFT size.
    N_FFT = myFFT.setup(settings, _N_FFT);  //hopefully, we got the same N_FFT that we asked for
    if (N_FFT < 1)
      return N_FFT;
    N_FFT = myIFFT.setup(settings, _N_FFT); //hopefully, we got the same N_FFT that we asked for
    if (N_FFT < 1)
      return N_FFT;

    //As we do a complex fft on a real signal, we only use half the returned FFT bins due
    // to conjugate symmetry. Store the number of bins to make it obvious and handy.
    N_bins = N_FFT / 2;

    //Spectral uses sqrtHann filtering
    (myFFT.getFFTObject())->useHanningWindow(); //applied prior to FFT

    //allocate memory to hold frequency domain data - complex r+i, so double the size of the
    // fft size.
    complex_2N_buffer = new (std::nothrow) float32_t[2 * N_FFT];
    if (complex_2N_buffer == NULL) return -1; 

    NR_X = new (std::nothrow) float32_t[N_bins];
    if (NR_X == NULL) return -1;
    ph1y = new (std::nothrow) float32_t[N_bins];
    if (ph1y == NULL) return -1;
    pslp = new (std::nothrow) float32_t[N_bins];
    if (pslp == NULL) return -1;
    xt = new (std::nothrow) float32_t[N_bins];
    if (xt == NULL) return -1;
    NR_SNR_post = new (std::nothrow) float32_t[N_bins];
    if (NR_SNR_post == NULL) return -1;
    NR_SNR_prio = new (std::nothrow) float32_t[N_bins];
    if (NR_SNR_prio == NULL) return -1;
    NR_Hk_old = new (std::nothrow) float32_t[N_bins];
    if (NR_Hk_old == NULL) return -1;
    NR_G = new (std::nothrow) float32_t[N_bins];
    if (NR_G == NULL) return -1;
    NR_Nest = new (std::nothrow) float32_t[N_bins];
    if (NR_Nest == NULL) return -1;
  }

  //Clear out and initialise
  for (int bindx = 0; bindx < N_bins; bindx++) {
    NR_Hk_old[bindx] = 0.1;     // old gain
    NR_Nest[bindx] = 0.01;
    NR_X[bindx] = 0.0;
    NR_SNR_post[bindx] = 2.0;
    NR_SNR_prio[bindx] = 1.0;
    NR_G[bindx] = 0.0;
  }

  //Work out the 'bin' range for our chosen voice frequencies
  // divide 2 to account for nyquist
  VAD_low = VAD_low_freq / ((sample_rate_Hz / 2.0) / (float32_t) (N_bins));
  VAD_high = VAD_high_freq / ((sample_rate_Hz / 2.0) / (float32_t) N_bins);

  xih1 = powf(10, asnr / 10.0);
  pfac = (1.0 / pspri - 1.0) * (1.0 + xih1);
  xih1r = 1.0 / (1.0 + xih1) - 1.0;

  //Configure the other things that might rely on the fft size of bitrate
  tinc = 1.0 / (sample_rate_Hz / AUDIO_BLOCK_SAMPLES);  //Frame time 
  tax = -tinc / log(tax_factor);       //noise output smoothing constant in seconds = -tinc/ln(0.8)
  tap = -tinc / log(tap_factor);       //speech prob smoothing constant in seconds = -tinc/ln(0.9)
  ap = expf(-tinc / tap);       //noise output smoothing factor
  ax = expf(-tinc / tax);       //noise output smoothing factor

  if (serial_debug) {
    Serial.println(" Spectral setup with fft:" + String(N_FFT));
    Serial.println("  FFT nblocks:" + String(myFFT.getNBuffBlocks()));
    Serial.println("  iFFT nblocks:" + String(myIFFT.getNBuffBlocks()));
    Serial.println("  Sample rate:" + String(sample_rate_Hz));
    Serial.println("  bins:" + String(N_bins));
    Serial.println("  VAD low:" + String(VAD_low));
    Serial.println("  VAD low freq:" + String(getVADLowFreq()));
    Serial.println("  VAD high:" + String(VAD_high));
    Serial.println("  VAD high freq:" + String(getVADHighFreq()));
    Serial.println("  tinc:" + String(tinc, 5));
    Serial.println("  tax_factor:" + String(tax_factor, 5));
    Serial.println("  tap_factor:" + String(tap_factor, 5));
    Serial.println("  tax:" + String(tax, 5));
    Serial.println("  tap:" + String(tap, 5));
    Serial.println("  ax:" + String(ax, 5));
    Serial.println("  ap:" + String(ap, 5));
    Serial.println("  xih1:" + String(xih1, 5));
    Serial.println("  xih1r:" + String(xih1r, 5));
    Serial.println("  pfac:" + String(pfac, 5));
    Serial.println("  snr_prio_min:" + String(getSNRPrioMin(), 5));
    Serial.println("  power_threshold:" + String(getPowerThreshold(), 5));
    Serial.println("  asnr:" + String(getAsnr(), 5));
    Serial.println("  NR_alpha:" + String(getNRAlpha(), 5));
    Serial.println("  NR_width:" + String(getNRWidth(), 5));

    Serial.flush();
  }

  enable(true);
  return is_enabled;
}

void AudioSpectralDenoise_F32::update(void)
{
  //get a pointer to the latest data
  audio_block_f32_t *in_audio_block = AudioStream_F32::receiveReadOnly_f32();
  if (!in_audio_block)
    return;

  //simply return the audio if this class hasn't been enabled
  if (!is_enabled) {
    AudioStream_F32::transmit(in_audio_block);
    AudioStream_F32::release(in_audio_block);
    return;
  }
  //******************************************************************************
  //convert to frequency domain
  //FFT is in complex_2N_buffer, interleaved real, imaginary, real, imaginary, etc
  myFFT.execute(in_audio_block, complex_2N_buffer);

  // Preserve the block id, so we can pass it out with our final result
  unsigned long incoming_id = in_audio_block->id;

  // We just passed ownership of in_audio_block to myFFT, so we can
  // release it here as we won't use it here again.
  AudioStream_F32::release(in_audio_block);

  if (init_phase == 1) {
    if (serial_debug) {
      Serial.println("One time init");
      Serial.flush();
    }
    init_phase++;

    for (int bindx = 0; bindx < N_bins; bindx++) {
      NR_G[bindx] = 1.0;
      NR_Hk_old[bindx] = 1.0;   // old gain or xu in development mode
      NR_Nest[bindx] = 0.0;
      pslp[bindx] = 0.5;
    }
  }
  //******************************************************************************
  //***** Calculate magnitude, used later for noise estimates and calculations
  // AIUI, as we are only passing real values into a complex FFT, the resulting
  // data contains duplicated mirrored data, thus we only need to evaluate the
  // magnitude of the first half of the bins, as it will be identical to that
  // of the second half of the bins. When we finally apply the NR results to the
  // FFT data we apply it to both the first half and the conjugate, mirror style.
  // Fundamentally, this saves us half the processing on some parts.
  for (int bindx = 0; bindx < N_bins; bindx++) {
    NR_X[bindx] =
        (complex_2N_buffer[bindx * 2] * complex_2N_buffer[bindx * 2] +
         complex_2N_buffer[bindx * 2 + 1] * complex_2N_buffer[bindx * 2 + 1]);
  }

  //Second stage initialisation
  if (init_phase == 2) {
    static int NR_init_counter = 0;
    if (serial_debug) {
      Serial.println("Two time init (" + String(NR_init_counter) + ")");
      Serial.flush();
    }
    for (int bindx = 0; bindx < N_bins; bindx++) {
      // we do it 20 times to average over 20 frames for app. 100ms only on
      // NR_on/bandswitch/modeswitch,...
      NR_Nest[bindx] = NR_Nest[bindx] + 0.05 * NR_X[bindx];
      xt[bindx] = psini * NR_Nest[bindx];
    }
    NR_init_counter++;
    if (NR_init_counter > 19)   //average over 20 frames for app. 100ms
    {
      if (serial_debug) {
        Serial.println("Two time init done");
        Serial.flush();
      }
      NR_init_counter = 0;
      init_phase++;
    }
    if (serial_debug)
      Serial.println(" Two time loop done");
  }

  //Now we are fully initialised, we can actually do the NR processing
  //******************************************************************************
  //MMSE (Minimum Mean Square Error) based noise estimate
  // code/algo inspired by the matlab based voicebox library:
  // http://www.ee.ic.ac.uk/hp/staff/dmb/voicebox/voicebox.html
  // Noise estimate code can be found at:
  // https://github.com/YouriT/matlab-speech/blob/master/MATLAB_CODE_SOURCE/voicebox/estnoiseg.m
  for (int bindx = 0; bindx < N_bins; bindx++) {
    float32_t xtr;

    // a-posteriori speech presence probability
    ph1y[bindx] = 1.0 / (1.0 + pfac * expf(xih1r * NR_X[bindx] / xt[bindx]));
    // smoothed speech presence probability
    pslp[bindx] = ap * pslp[bindx] + (1.0 - ap) * ph1y[bindx];

    // limit ph1y
    if (pslp[bindx] > psthr) {
      ph1y[bindx] = 1.0 - pnsaf;
    } else {
      ph1y[bindx] = fmin(ph1y[bindx], 1.0);
    }
    // estimated raw noise spectrum
    xtr = (1.0 - ph1y[bindx]) * NR_X[bindx] + ph1y[bindx] * xt[bindx];
    // smooth the noise estimate
    xt[bindx] = ax * xt[bindx] + (1.0 - ax) * xtr;
  }

  // Limit the ratios
  // I don't have a lot of info on how this works, but SNRpost and SNRprio are related
  // to both Ephraim&Malah(84) and Romanin(2009) papers
  for (int bindx = 0; bindx < N_bins; bindx++) {
    // limited to +30 /-15 dB, might be still too much of reduction, let's try it?
    NR_SNR_post[bindx] = fmax(fmin(NR_X[bindx] / xt[bindx], 1000.0), snr_prio_min);

    NR_SNR_prio[bindx] =
      fmax(NR_alpha * NR_Hk_old[bindx] +
      (1.0 - NR_alpha) * fmax(NR_SNR_post[bindx] - 1.0, 0.0), 0.0);
  }

    //******************************************************************************
    // VAD
    // maybe we should limit this to the signal containing bins (filtering!!)
    for (int bindx = VAD_low; bindx < VAD_high; bindx++) {
      float32_t v =
          NR_SNR_prio[bindx] * NR_SNR_post[bindx] / (1.0 + NR_SNR_prio[bindx]);
      NR_G[bindx] = 1.0 / NR_SNR_post[bindx] * sqrtf((0.7212 * v + v * v));
      NR_Hk_old[bindx] = NR_SNR_post[bindx] * NR_G[bindx] * NR_G[bindx];
    }

    //******************************************************************************
    // Do the musical noise reduction
    // musical noise "artefact" reduction by dynamic averaging - depending on SNR ratio
    pre_power = 0.0;
    post_power = 0.0;
    for (int bindx = VAD_low; bindx < VAD_high; bindx++) {
      pre_power += NR_X[bindx];
      post_power += NR_G[bindx] * NR_G[bindx] * NR_X[bindx];
    }

    power_ratio = post_power / pre_power;
    if (power_ratio > power_threshold) {
      power_ratio = 1.0;
      NN = 1;
    } else {
      NN = 1 + 2 * (int)(0.5 +
                         NR_width * (1.0 - power_ratio / power_threshold));
    }

    for (int bindx = VAD_low + NN / 2; bindx < VAD_high - NN / 2; bindx++) {
      NR_Nest[bindx] = 0.0;
      for (int m = bindx - NN / 2; m <= bindx + NN / 2; m++) {
        NR_Nest[bindx] += NR_G[m];
      }
      NR_Nest[bindx] /= (float32_t) NN;
    }

    // and now the edges - only going NN steps forward and taking the average
    // lower edge
    for (int bindx = VAD_low; bindx < VAD_low + NN / 2; bindx++) {
      NR_Nest[bindx] = 0.0;
      for (int m = bindx; m < (bindx + NN); m++) {
        NR_Nest[bindx] += NR_G[m];
      }
      NR_Nest[bindx] /= (float32_t) NN;
    }

    // upper edge - only going NN steps backward and taking the average
    for (int bindx = VAD_high - NN; bindx < VAD_high; bindx++) {
      NR_Nest[bindx] = 0.0;
      for (int m = bindx; m > (bindx - NN); m--) {
        NR_Nest[bindx] += NR_G[m];
      }
      NR_Nest[bindx] /= (float32_t) NN;
    }
    // end of edge treatment

    for (int bindx = VAD_low + NN / 2; bindx < VAD_high - NN / 2; bindx++) {
      NR_G[bindx] = NR_Nest[bindx];
    }
    // end of musical noise reduction

  //******************************************************************************
  // And finally actually apply the weightings to the signals...
  // FINAL SPECTRAL WEIGHTING: Multiply current FFT results with complex_2N_buffer for
  // bins with the bin-specific gain factors G
  for (int bindx = 0; bindx < N_bins; bindx++) {
    // real part
    complex_2N_buffer[bindx * 2] = complex_2N_buffer[bindx * 2] * NR_G[bindx];

    // imag part
    complex_2N_buffer[bindx * 2 + 1] =
        complex_2N_buffer[bindx * 2 + 1] * NR_G[bindx];

    // real part conjugate symmetric
    //N_bins * 4 == N_FFT * 2 == N_FFT[real, imag]
    complex_2N_buffer[N_bins * 4 - bindx * 2 - 2] =
        complex_2N_buffer[N_bins * 4 - bindx * 2 - 2] * NR_G[bindx];

    // imag part conjugate symmetric
    complex_2N_buffer[N_bins * 4 - bindx * 2 - 1] =
        complex_2N_buffer[N_bins * 4 - bindx * 2 - 1] * NR_G[bindx];
  }

  //******************************************************************************
  //And finally call the IFFT, back to the time domain, and pass the processed block on

  //out_block is pre-allocated in here.
  audio_block_f32_t *out_audio_block = myIFFT.execute(complex_2N_buffer);

  //update the block number to match the incoming one
  out_audio_block->id = incoming_id;

  //send the returned audio block.  Don't issue the release command here because myIFFT will re-use it
  //don't release this buffer because myIFFT re-uses it within its own code
  AudioStream_F32::transmit(out_audio_block); //don't release this buffer because myIFFT re-uses it within its own code

  return;
}
