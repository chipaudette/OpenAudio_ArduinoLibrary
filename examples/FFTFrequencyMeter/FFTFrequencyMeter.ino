/* FFTFrequencyMeter.ino
 * Bob Larkin  20 Feb 2021    Pblic Domain
 * 
 * This INO demonstrates the use of FFT measurement of frequency.
 * It generates a known, random frequency between 300 and 3000 Hz,
 * measures that frequency with FFT interpolation, and prints the
 * two frequencies and the difference between them (the error).
 * This is mathematically correct for the Hann window only. A run
 * of 400 samples of this INO showed:
 *    Average error = -0.0003 Hz
 *    Standard Deviation = 0.0247 Hz
 *    Maximum +/- error 0.042 Hz
 * The method was generously provided by DerekR 
 * https://forum.pjrc.com/threads/36358-A-New-Accurate-FFT-Interpolator-for-Frequency-Estimation
 * 
 * Homework: Create a known S/N by adding a SynthGaussianWhiteNoise
 * object combined along with the sine wave by an AudioMixer_F32 object.
 * Cut the Serial output and paste it
 * into your favorite spread sheet.  Find the power S/N where
 * the standard deviation of the error rises to a tenth of the 44100/1024
 * = 43 Hz bin spacing.
 * 
 * This INO was originally put together to demonstrate the use of the
 * myFFT.getData(); that returns a pointer to the FFT output data.  See
 * findFrequency() below.
 * 
 * The Knuth and Lewis random noise generator might be useful elsewhere.
 */

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include "Audio.h"

AudioSynthWaveformSine_F32   wv;
AudioAnalyzeFFT1024_F32      myFFT;
AudioOutputI2S_F32           i2sOut;
AudioConnection_F32          patchCord1(wv, 0,  myFFT, 0);

void setup(){
  Serial.begin(9600);
  delay(1000);
  AudioMemory_F32(20);
  Serial.println("FFT Frequency Meter");
  Serial.println("Actual  Measured  Difference");

  wv.amplitude(0.5);         // Initialize Waveform Generator
  wv.frequency(1000);

  myFFT.setOutputType(FFT_POWER);
  myFFT.windowFunction(AudioWindowHanning1024);
  }

void loop() {
  static float sineFrequency, fftFrequency;

  Serial.print(sineFrequency, 3);  Serial.print(", ");
  fftFrequency = findFrequency();
  Serial.print(fftFrequency, 3);   Serial.print(", ");
  Serial.println(fftFrequency - sineFrequency,  3);

  // Find a new frequency for the next time around the loop.
  sineFrequency = 300.0f + 2700.0f*uniformRandom();
  wv.frequency(sineFrequency);
  delay(250);  // Let things settle
  }

  // Return the estimated frequency, based on powers in FFT bins.
  // Following from DerekR
  // https://forum.pjrc.com/threads/36358-A-New-Accurate-FFT-Interpolator-for-Frequency-Estimation
  // " 1) A record of length 1024 samples is windowed with a Hanning window
  //   2) The magnitude spectrum is computed from the FFT, and the two (adjacent)
  //      largest amplitude lines are found.  Let the largest be line L, and the
  //      other be either L+1, of L-1.
  //   3) Compute the ratio R of the amplitude of the two largest lines.
  //   4) If the amplitude of L+1 is greater than L-1 then
  //              f = (L + (2-R)/(1+R))*f_sample/1024
  //        otherwise
  //              f = (L - (2-R)/(1+R))*f_sample/1024  "
  float findFrequency(void)  {
    float specMax = 0.0f;
    uint16_t iiMax = 0;

    // Get pointer to data array of powers, float output[512];
    float* pPwr = myFFT.getData();
    // Find biggest bin
    for(int ii=2; ii<510; ii++)  {
        if (*(pPwr + ii) > specMax) { // Find highest peak of 512
          specMax = *(pPwr + ii);
          iiMax = ii;
          }
        }
      float vm = sqrtf( *(pPwr + iiMax - 1) );
      float vc = sqrtf( *(pPwr + iiMax) );
      float vp = sqrtf( *(pPwr + iiMax + 1) );
      if(vp > vm)  {
         float R = vc/vp;
         return ( (float32_t)iiMax + (2-R)/(1+R) )*44100.0f/1024.0f;
         }
      else  {
         float R = vc/vm;
         return ( (float32_t)iiMax - (2-R)/(1+R) )*44100.0f/1024.0f;
         }
  }

#define FL_ONE  0X3F800000
#define FL_MASK 0X007FFFFF
  /* The "Even Quicker" uniform random sample generator from D. E. Knuth and
   * H. W. Lewis and described in Chapter 7 of "Numerical Receipes in C",
   * 2nd ed, with the comment "this is about as good as any 32-bit linear
   * congruential generator, entirely adequate for many uses."
   */
  float uniformRandom(void) {
    static uint32_t idum = 54321;
    union {
       uint32_t  i32;
       float32_t f32;
       } uinf;
    idum = (uint32_t)1664525 * idum + (uint32_t)1013904223;
    uinf.i32 = FL_ONE | (FL_MASK & idum);  // Generate random number
    return uinf.f32 - 1.0f;  // resulting uniform deviate on (0.0, 1.0)
    }
