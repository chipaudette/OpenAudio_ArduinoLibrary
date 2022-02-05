/**
  ******************************************************************************
  * @file    AudioFilterConvolution_F32.cpp
  * @author  Giuseppe Callipo - IK8YFW - ik8yfw@libero.it
  * @version V1.0.0
  * @date    02-05-2021
  * @brief   F32 Filter Convolution
  *
  ******************************************************************************
  ******************************************************************************
   This software is based on the  AudioFilterConvolution routine
   Written by Brian Millier on Mar 2017

   https://circuitcellar.com/research-design-hub/fancy-filtering-with-the-teensy-3-6/
   and modified by Giuseppe Callipo - ik8yfw.
   Modifications:
    1) Class refactoring, change some methods visibility;
    2) Filter coefficients calculation included into class;
    3) Change the class for running in both with F32
      OpenAudio_ArduinoLibrary for Teensy;
    4) Added initFilter method for single anf fast initialization and on
       the fly reinititializzation;
   5) Optimize it to use as output audio filter on SDR receiver.
  *******************************************************************/

#include "AudioFilterConvolution_F32.h"

boolean AudioFilterConvolution_F32::begin(int status)
{
  enabled = status;
  return(true);
}

void AudioFilterConvolution_F32::passThrough(int stat)
{
  passThru=stat;
}

void AudioFilterConvolution_F32::impulse(float32_t *FIR_coef) {
//  arm_q15_to_float(coefs, FIR_coef, 513); // convert int_buffer to float 32bit
  int k = 0;
  int i = 0;
  enabled = 0; // shut off audio stream while impulse is loading
  for (i = 0; i < (FFT_length / 2) + 1; i++)
  {
    FIR_filter_mask[k++] = FIR_coef[i];
    FIR_filter_mask[k++] = 0;
  }

  for (i = FFT_length + 1; i < FFT_length * 2; i++)
  {
    FIR_filter_mask[i] = 0.0;
  }
  arm_cfft_f32( &arm_cfft_sR_f32_len1024, FIR_filter_mask, 0, 1);
  for (int i = 0; i < 1024; i++) {
  //  Serial.println(FIR_filter_mask[i] * 32768);
  }
  // for 1st time thru, zero out the last sample buffer to 0
  memset(last_sample_buffer_L, 0, sizeof(last_sample_buffer_L));
  state = 0;
  enabled = 1;  //enable audio stream again
}

void AudioFilterConvolution_F32::update(void)
{
  audio_block_f32_t *block;
  float32_t *bp;

  if (enabled != 1 ) return;
  block = receiveWritable_f32(0);   // MUST be Writable, as convolution results are written into block
  if (block) {
    switch (state) {
    case 0:
      bp = block->data;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        buffer[i] = *bp++;
      }
      bp = block->data;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        *bp++ = tbuffer[i];   // tbuffer contains results of last FFT/multiply/iFFT processing (convolution filtering)
      }
      AudioStream_F32::transmit(block);
      AudioStream_F32::release(block);
      state = 1;
      break;
    case 1:
      bp = block->data;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        buffer[128+i] = *bp++;
      }
      bp = block->data;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        *bp++ = tbuffer[i+128]; // tbuffer contains results of last FFT/multiply/iFFT processing (convolution filtering)
      }
      AudioStream_F32::transmit(block);
      AudioStream_F32::release(block);
      state = 2;
      break;
    case 2:
      bp = block->data;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        buffer[256 + i] = *bp++;
      }
      bp = block->data;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        *bp++ = tbuffer[i+256];  // tbuffer contains results of last FFT/multiply/iFFT processing (convolution filtering)
      }
      AudioStream_F32::transmit(block);
      AudioStream_F32::release(block);
      state = 3;
      break;
    case 3:
      bp = block->data;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        buffer[384 + i] = *bp++;
      }
      bp = block->data;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        *bp++ = tbuffer[i + 384];  // tbuffer contains results of last FFT/multiply/iFFT processing (convolution filtering)
      }
      AudioStream_F32::transmit(block);
      AudioStream_F32::release(block);
      state = 0;
      // 4 blocks are in- now do the FFT1024,complex multiply and iFFT1024  on 512samples of data
      // using the overlap/add method
      // 1st convert Q15 samples to float
      //arm_q15_to_float(buffer, float_buffer_L, 512);
      arm_copy_f32 (buffer, float_buffer_L, 512);
       // float_buffer samples are now standardized from > -1.0 to < 1.0
      if (passThru ==0) {
        memset(FFT_buffer + 1024, 0, sizeof(FFT_buffer) / 2);    // zero pad last half of array- necessary to prevent aliasing in FFT
        //fill FFT_buffer with current audio samples
        k = 0;
        for (i = 0; i < 512; i++)
        {
          FFT_buffer[k++] = float_buffer_L[i];   // real
          FFT_buffer[k++] = float_buffer_L[i];   // imag
        }
        // calculations are performed in-place in FFT routines
        arm_cfft_f32(&arm_cfft_sR_f32_len1024, FFT_buffer, 0, 1);// perform complex FFT
        arm_cmplx_mult_cmplx_f32(FFT_buffer, FIR_filter_mask, iFFT_buffer, FFT_length);   // complex multiplication in Freq domain = convolution in time domain
        arm_cfft_f32(&arm_cfft_sR_f32_len1024, iFFT_buffer, 1, 1);  // perform complex inverse FFT
        k = 0;
        l = 1024;
        for (int i = 0; i < 512; i++) {
          float_buffer_L[i] = last_sample_buffer_L[i] + iFFT_buffer[k++];   // this performs the "ADD" in overlap/Add
          last_sample_buffer_L[i] = iFFT_buffer[l++];       // this saves 512 samples (overlap) for next time around
          k++;
          l++;
        }
      } //end if passTHru
      // convert floats to Q15 and save in temporary array tbuffer
      //arm_float_to_q15(&float_buffer_L[0], &tbuffer[0], BUFFER_SIZE*4);
      arm_copy_f32 (&float_buffer_L[0], &tbuffer[0], BUFFER_SIZE*4);
      break;
    }
  }
}

float32_t AudioFilterConvolution_F32::Izero (float32_t x)
{
    float32_t x2 = x / 2.0;
    float32_t summe = 1.0;
    float32_t ds = 1.0;
    float32_t di = 1.0;
    float32_t errorlimit = 1e-9;
    float32_t tmp;
    do
    {
        tmp = x2 / di;
        tmp *= tmp;
        ds *= tmp;
        summe += ds;
        di += 1.0;
    }   while (ds >= errorlimit * summe);
    return (summe);
}  // END Izero

float AudioFilterConvolution_F32::m_sinc(int m, float fc)
{  // fc is f_cut/(Fsamp/2)
  // m is between -M and M step 2
  //
  float x = m*PIH;
  if(m == 0)
    return 1.0f;
  else
    return sinf(x*fc)/(fc*x);
}

void AudioFilterConvolution_F32::calc_FIR_coeffs (float * coeffs, int numCoeffs, float32_t fc, float32_t Astop, int type, float dfc, float Fsamprate)
    // pointer to coefficients variable, no. of coefficients to calculate, frequency where it happens, stopband attenuation in dB,
    // filter type, half-filter bandwidth (only for bandpass and notch)
 {  // modified by WMXZ and DD4WH after
    // Wheatley, M. (2011): CuteSDR Technical Manual. www.metronix.com, pages 118 - 120, FIR with Kaiser-Bessel Window
    // assess required number of coefficients by
    //     numCoeffs = (Astop - 8.0) / (2.285 * TPI * normFtrans);
    // selecting high-pass, numCoeffs is forced to an even number for better frequency response
     int ii,jj;
     float32_t Beta;
     float32_t izb;
     float fcf = fc;
     int nc = numCoeffs;
     fc = fc / Fsamprate;
     dfc = dfc / Fsamprate;
     // calculate Kaiser-Bessel window shape factor beta from stop-band attenuation
     if (Astop < 20.96)
       Beta = 0.0;
     else if (Astop >= 50.0)
       Beta = 0.1102 * (Astop - 8.71);
     else
       Beta = 0.5842 * powf((Astop - 20.96), 0.4) + 0.07886 * (Astop - 20.96);

     izb = Izero (Beta);
     if(type == 0) // low pass filter
//     {  fcf = fc;
     {  fcf = fc * 2.0;
      nc =  numCoeffs;
     }
     else if(type == 1) // high-pass filter
     {  fcf = -fc;
      nc =  2*(numCoeffs/2);
     }
     else if ((type == 2) || (type==3)) // band-pass filter
     {
       fcf = dfc;
       nc =  2*(numCoeffs/2); // maybe not needed
     }
     else if (type==4)  // Hilbert transform
   {
         nc =  2*(numCoeffs/2);
       // clear coefficients
       for(ii=0; ii< 2*(nc-1); ii++) coeffs[ii]=0;
       // set real delay
       coeffs[nc]=1;

       // set imaginary Hilbert coefficients
       for(ii=1; ii< (nc+1); ii+=2)
       {
         if(2*ii==nc) continue;
       float x =(float)(2*ii - nc)/(float)nc;
       float w = Izero(Beta*sqrtf(1.0f - x*x))/izb; // Kaiser window
       coeffs[2*ii+1] = 1.0f/(PIH*(float)(ii-nc/2)) * w ;
       }
       return;
   }

     for(ii= - nc, jj=0; ii< nc; ii+=2,jj++)
     {
       float x =(float)ii/(float)nc;
       float w = Izero(Beta*sqrtf(1.0f - x*x))/izb; // Kaiser window
       coeffs[jj] = fcf * m_sinc(ii,fcf) * w;
     }

     if(type==1)
     {
       coeffs[nc/2] += 1;
     }
     else if (type==2)
     {
       for(jj=0; jj< nc+1; jj++) coeffs[jj] *= 2.0f*cosf(PIH*(2*jj-nc)*fc);
     }
     else if (type==3)
     {
       for(jj=0; jj< nc+1; jj++) coeffs[jj] *= -2.0f*cosf(PIH*(2*jj-nc)*fc);
       coeffs[nc/2] += 1;
     }
} // END calc_FIR_coef



void AudioFilterConvolution_F32::initFilter ( float32_t fc, float32_t Astop, int type, float dfc){
    //Init Fir
    calc_FIR_coeffs (FIR_Coef, MAX_NUMCOEF, fc, Astop, type, dfc, fs);
    begin(0);   // set to zero to disable audio processing until impulse has been loaded
    impulse(FIR_Coef);  // generates Filter Mask and enables the audio stream
}
