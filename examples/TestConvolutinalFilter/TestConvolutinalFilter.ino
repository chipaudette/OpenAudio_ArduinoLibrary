/*
 * TestConvolutionalFilter.ino   Bob Larkin 2 Feb 2022
 * Measure frequency response of convolution filtering.
 *
 * This uses the Goertzel algoritm of AudioAnalyzeToneDetect_F32 as
 * a "direct conversion receiver."  This provides the excellent
 * selectivity needed for measuring below -100 dBfs.
 *
 * Comparison with 512 tap FIR filtering is included, but double slash
 * commented out.
 *
 *  Public Domain - Teensy
 */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include "AudioFilterConvolution_F32.h"

AudioSynthSineCosine_F32 sine_cos1;          //xy=87,151
AudioFilterConvolution_F32 convFilt1;     //xy=163,245
//AudioFilterFIRGeneral_F32 filterFIRgeneral1; //xy=285,177
AudioOutputI2S_F32       audioOutI2S1;       //xy=357,337
//AudioAnalyzeToneDetect_F32 toneDet1;         //xy=377,259
AudioAnalyzeToneDetect_F32 toneDet2;         //xy=378,299
//AudioConnection_F32          patchCord1(sine_cos1, 0, filterFIRgeneral1, 0);
AudioConnection_F32          patchCord2(sine_cos1, 1, convFilt1, 0);
AudioConnection_F32          patchCord3(convFilt1, toneDet2);
AudioConnection_F32          patchCord4(convFilt1, 0, audioOutI2S1, 0);
//AudioConnection_F32          patchCord5(filterFIRgeneral1, 0, toneDet1, 0);
AudioControlSGTL5000     sgtl5000_1;         //xy=164,343

// CAUTION that audio output is intended for an oscilloscope and may be very
// VERY loud in a speaker or headphone.

//float32_t coeffFIR[512];
//float32_t stateFIR[1160];
//float32_t attenFIR[256];

void setup(void) {
  AudioMemory(5);
  AudioMemory_F32(50);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test Convolutional Filtering routine for F32 ***");
  sine_cos1.frequency(1000.0f);
  sine_cos1.amplitude(1.0f);
//  toneDet1.frequency(100.0f, 5);
  toneDet2.frequency(100.0f, 5);

  //  *** Un-comment just ONE of the following filters  ***
     // Low Pass Filter, 4000 Hz cut off, 60 dB first sidelobe
     convFilt1.initFilter(4000.0f, 60.0f, LOWPASS, 0.0f);

     // High Pass Filter, 2000 Hz cut off, 25 dB first sidelobe
     //convFilt1.initFilter(2000.0f, 25.0f, HIGHPASS, 0.0f);

     // IK8YFW CW - Centered at 800Hz, 40 db SL, 2=BPF, width = 1200Hz
     //convFilt1.initFilter(800.0f, 40.0f, BANDPASS, 1200.0f);

     // IK8YFW SSB - Centered at 1500Hz, 60 db SL, 2=BPF, width = 3000Hz
     //convFilt1.initFilter(1500.0f, 60.0f, BANDPASS, 3000.0f);

     // Band Reject filter, Centered at 6000Hz, 50 db SL, 3=BRF, width = 2000Hz
     //convFilt1.initFilter(6000.0f, 60.0f, BANDREJECT, 2000.0f);

     // BP filter, Centered at 6000Hz, 50 db SL, 2=BPF, width = 2000Hz
     //convFilt1.initFilter(6000.0f, 60.0f, BANDPASS, 2000.0f);


  // Design for direct FIR.  This sets frequency response.
  // Bin for 4kHz at 44.117kHz sample rate and 400 coeff's is (4000/44117) * (512/2) = 23.2
//  for(int ii=0; ii<47; ii++)    attenFIR[ii] = 0.0f;
//  for(int ii=47; ii<256; ii++)  attenFIR[ii] = -150.0f;
  // FIRGeneralNew(float *adb, uint16_t nFIR, float *cf32f, float kdb, float *pStateArray);
  // filterFIRgeneral1.FIRGeneralNew(attenFIR, 512, coeffFIR, 60.0, stateFIR);

  // DSP measure frequency response in dB  uniformly spaced from 100 to 20000 Hz
  Serial.println("Freq Hz  Conv dB  FIR dB");
  for(float32_t f=100.0; f<=20000.0f; f+=50.0f)
     {
     sine_cos1.frequency(f);
//     toneDet1.frequency(f, (f>2000.0f ? 100 : 5));
     toneDet2.frequency(f, (f>2000.0f ? 100 : 5));
     delay(50);
//     while(!toneDet1.available())delay(2) ;
//     toneDet1.read();
     while(!toneDet2.available())delay(2) ;
     toneDet2.read();
     while(!toneDet2.available())delay(2) ;
     Serial.print(f);
     Serial.print(", ");
     while(!toneDet2.available())delay(2) ;
     Serial.println(20.0f*log10f(toneDet2.read() ) );
     }
  }

void loop() {
}
