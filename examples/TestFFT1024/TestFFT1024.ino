
#if 0

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"
#include "mathDSP_F32.h"
#if defined(__IMXRT1062__)
#include "arm_const_structs.h"
#endif

#define NFFT 1024
#define NFFT_D2 NFFT/2

#define FFT_PI 3.14159265359f
#define PI2 2.0f*FFT_PI

void setup(void) {
    float x[NFFT];       // Real DFT input
    float Xre[NFFT], Xim[NFFT]; // DFT of x
    float P[NFFT];           // power spectrum of x
    float kf, nf;
    float fft_buffer[2*NFFT];  // 2 is fo CMSIS FFT
    float sinN[NFFT_D2];
    float cosN[NFFT_D2];

    uint32_t tt;
    // Instantiate FFT, T4.x ONLY
    arm_cfft_instance_f32   Sfft;
    Sfft = arm_cfft_sR_f32_len1024;

   // Instantiate FFT, T4.x ONLY
    arm_cfft_instance_f32   Sfft_128;
    Sfft_128 = arm_cfft_sR_f32_len512;

    Serial.begin(300); // Any speed works
    delay(1000);

    // Factors for using half size complex FFT
    for(int n=0; n<NFFT_D2; n++)  {
       sinN[n] = sinf(FFT_PI*((float)n)/((float)NFFT_D2));
       cosN[n] = cosf(FFT_PI*((float)n)/((float)NFFT_D2));
       }

    // The signal, a sine wave that fits integer number (2 here) of times
    Serial.println("The input waveform, NFFT points");
    for (int n=0; n<NFFT; n++) {
        x[n] = sinf(5.0f*FFT_PI*((float)n)/((float)NFFT_D2));
        Serial.println(x[n], 8);
        }
    Serial.println();

    Serial.println("The DFT by full NxN DFT, k, Real, Imag, Power");
    // Calculate DFT of x[n] with NFFT point DFT
    for (int k=0 ; k<NFFT ; ++k) {
        kf = (float)k;
        // Real part of X[k]
        Xre[k] = 0.0f;
        Xim[k] = 0.0f;
        for (int n=0 ; n<NFFT ; ++n)  {
           nf = (float)n;
           Xre[k] += x[n] * cos(nf * kf * PI2 / ((float)NFFT));
           Xim[k] -= x[n] * sin(nf * kf * PI2 / ((float)NFFT));
           }
        // Power at kth frequency bin
        P[k] = 10.0f*log10f(Xre[k]*Xre[k] + Xim[k]*Xim[k]);
        Serial.print(k); Serial.print(",");
        Serial.print(Xre[k],8); Serial.print(",");
        Serial.print(Xim[k],8); Serial.print(",");
        Serial.println(P[k],3);
        }


    Serial.println("And now the same thing with FFT size NFFT/2");

    for (int k = 0; k < NFFT_D2; k++) {  // For each output element
        kf = (float)k;
        float sumreal = 0;
        float sumimag = 0;
        for (int n = 0; n < NFFT_D2; n++) {  // For each input element
            nf = (float)n;
            float angle = PI2 * nf * kf / ((float)NFFT_D2);
            sumreal +=  x[2*n] * cos(angle) + x[2*n+1] * sin(angle);
            sumimag += -x[2*n] * sin(angle) + x[2*n+1] * cos(angle);
        }
        Xre[k] = sumreal;
        Xim[k] = sumimag;
    }

    // Rearrange the outputs to look like the FFT
    for(int k=0; k<NFFT_D2; k++) {
       fft_buffer[2*k] = Xre[k];
       fft_buffer[2*k+1] = Xim[k];
       }


    // Now the post FT processing for using half-length transform
    Xre[0] = 0.0f;
    for(int n=0; n<NFFT; n++)
       Xre[0] += x[n]/((float)NFFT);    // DC real
    Xim[0] = 0.0f;                      // DC Imag
    P[0] = 10.0f*log10f(Xre[0]*Xre[0]);
    // And the non-DC values
    for(int i=1; i<NFFT_D2; i++)  {
        float rns = 0.5f*(fft_buffer[2*i] + fft_buffer[NFFT-2*i]);
        float ins = 0.5f*(fft_buffer[2*i+1] + fft_buffer[NFFT-2*i+1]);
        float rnd = 0.5f*(fft_buffer[2*i] - fft_buffer[NFFT-2*i]);
        float ind = 0.5f*(fft_buffer[2*i+1] - fft_buffer[NFFT-2*i+1]);
        Xre[i] = rns + cosN[i]*ins - sinN[i]*rnd;
        Xim[i] = ind - sinN[i]*ins - cosN[i]*rnd;
        P[i] = 10.0f*log10f(Xre[i]*Xre[i] + Xim[i]*Xim[i]);
        }

    for(int k=0; k<NFFT_D2; k++)  {
        Serial.print(k); Serial.print(",");
        Serial.print(Xre[k],8); Serial.print(",");
        Serial.print(Xim[k],8); Serial.print(",");
        Serial.println(P[k],3);
        }
    Serial.println();


    // And the same data with the CMSIS FFT
    Serial.println("And now the same thing with CMSIS FFT size NFFT, 0.0 input for imag");
        // Teensyduino core for T4.x supports arm_cfft_f32
        // arm_cfft_f32 (const arm_cfft_instance_f32 *S, float32_t *p1, uint8_t ifftFlag, uint8_t bitReverseFlag)

        for(int k=0; k<NFFT; k++)  {
            fft_buffer[2*k] = x[k];
            fft_buffer[2*k+1] = 0.0f;
            }
        tt = micros();
        arm_cfft_f32 (&Sfft, fft_buffer, 0, 1);
        Serial.print("NFFT length FFT uSec = ");
        Serial.println(micros()-tt);
        for (int i=0; i < NFFT; i++) {
            float magsq = fft_buffer[2*i]*fft_buffer[2*i] + fft_buffer[2*i+1]*fft_buffer[2*i+1];
            Serial.print(i); Serial.print(",");
            // Serial.print(Xre[k],8); Serial.print(",");
            // Serial.print(Xim[k],8); Serial.print(",");
            Serial.println(10.0f*log10f(magsq), 3);
            }


    // And the same data with the CMSIS FFT of size NFFT/2 and interleaved Trig sorting
    // Time: for NFFT=256, this method is 18 uSec, down from 23 uSec using 256 length
    // and 0's in the imag inputs.  Memory use is about half and accuracy is the same.
    Serial.println("And now with CMSIS FFT size NFFT/2, 0.0 input for imag");
    // Teensyduino core for T4.x supports arm_cfft_f32
    // arm_cfft_f32 (const arm_cfft_instance_f32 *S, float32_t *p1, uint8_t ifftFlag, uint8_t bitReverseFlag)

    for(int k=0; k<NFFT; k++)  {
        fft_buffer[k] = x[k];
        }
    arm_cfft_f32 (&Sfft_128, fft_buffer, 0, 1);

    // Now the post FT processing for using half-length transform
    Xre[0] = 0.0f;
    for(int n=0; n<NFFT; n++)
       Xre[0] += x[n]/((float)NFFT);    // DC real
    Xim[0] = 0.0f;                      // DC Imag
    P[0] = 10.0f*log10f(Xre[0]*Xre[0]);
    // And the non-DC values
    for(int i=1; i<NFFT_D2; i++)  {
        float rns = 0.5f*(fft_buffer[2*i] + fft_buffer[NFFT-2*i]);
        float ins = 0.5f*(fft_buffer[2*i+1] + fft_buffer[NFFT-2*i+1]);
        float rnd = 0.5f*(fft_buffer[2*i] - fft_buffer[NFFT-2*i]);
        float ind = 0.5f*(fft_buffer[2*i+1] - fft_buffer[NFFT-2*i+1]);
        Xre[i] = rns + cosN[i]*ins - sinN[i]*rnd;
        Xim[i] = ind - sinN[i]*ins - cosN[i]*rnd;
        P[i] = 10.0f*log10f(Xre[i]*Xre[i] + Xim[i]*Xim[i]);
        }
    for(int k=0; k<NFFT_D2; k++)  {
        Serial.print(k); Serial.print(",");
        Serial.print(Xre[k],8); Serial.print(",");
        Serial.print(Xim[k],8); Serial.print(",");
        Serial.println(P[k],3);
        }
    Serial.println();
    }

void loop(void)  {

}
#endif


// *****************************************************************
// *****************************************************************

// TestFFT1024.ino       Bob Larkin  W7PUA
// Started from PJRC Teensy  Examples/Audio/Analysis/FFT
//
// Compute a 1024 point Fast Fourier Transform (spectrum analysis)
// on audio connected to the Left Line-In pin.  By changing code,
// a synthetic sine wave can be input instead.
//
// The power output from 512 frequency analysis bins are printed to
// the Arduino Serial Monitor.  The format is selectable.
// Output power averaging is an option
//
// T4.0: Uses 11.5% processor and 9 F32 memory blocks, both max.
//
// This example code is in the public domain.

#include <Audio.h>
#include "OpenAudio_ArduinoLibrary.h"

// Create the Audio components.  These should be created in the
// order data flows, inputs/sources -> processing -> outputs
//
// AudioInputI2S_F32          audioInput;  // audio shield: mic or line-in
AudioSynthSineCosine_F32   sinewave;
AudioAnalyzeFFT1024_F32    myFFT;
AudioOutputI2S_F32         audioOutput; // audio shield: headphones & line-out NU
// Connect either the live input or synthesized sine wave
// AudioConnection_F32        patchCord1(audioInput, 0, myFFT, 0);
AudioConnection_F32        patchCord1(sinewave, 0, myFFT, 0);
AudioControlSGTL5000       audioShield;
float xxx[1024];
uint32_t ct = 0;
uint32_t count = 0;

void setup() {
  Serial.begin(300); // Any speed works
  delay(1000);

  AudioMemory_F32(20);

  // Enable the audio shield and set the output volume.
  audioShield.enable();
  audioShield.inputSelect(AUDIO_INPUT_LINEIN);

  // Create a synthetic sine wave, for testing
  // To use this, edit the connections above
  // sinewave.frequency(1033.99f);   // Bin 24  T3.x
  // sinewave.frequency(1033.59375f);   // Bin 24  T4.x at 44100
  // sinewave.frequency(1055.0f);  // Bin 24.5, demonstrates windowing
  // Or some random frequency:
  sinewave.frequency(1234.5f);

  sinewave.amplitude(1.0f);

  // Set windowing function
  // myFFT.windowFunction(AudioWindowNone);
  // myFFT.windowFunction(AudioWindowHanning1024);  // default
  // The next Kaiser window needs a dB peak sidelobe number
  // myFFT.windowFunction(AudioWindowKaiser1024, 70.0f);
  myFFT.windowFunction(AudioWindowBlackmanHarris1024);

  // To print the window function:
  // float* pw=myFFT.getWindow();
  // for(int jj=0; jj<1024; jj++)
  //    Serial.println(*pw++, 6);

  myFFT.setNAverage(1);

  myFFT.setOutputType(FFT_DBFS);   // FFT_RMS or FFT_POWER or FFT_DBFS
}

void loop() {
  if (myFFT.available() /*&& ++ct == 4*/ ) {
     // each time new FFT data is available
     // print it all to the Arduino Serial Monitor

     float* pin = myFFT.getData();
     for (int  gg=0; gg<512; gg++)
        xxx[gg]= *(pin + gg);
     for (int i=0; i<512; i++) {
        Serial.print(i);
        Serial.print(",  ");
        Serial.println(xxx[i], 8);
        }
     Serial.println();
     }

  /*
  if(count++<200) {
     Serial.print("CPU: Max Percent Usage: ");
     Serial.println(AudioProcessorUsageMax());
     Serial.print("   Max Float 32 Memory: ");
     Serial.println(AudioMemoryUsageMax_F32());
     }
   */
  }

