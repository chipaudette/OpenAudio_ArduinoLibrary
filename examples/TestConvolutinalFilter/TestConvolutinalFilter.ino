/*
 * TestConvolutionalFilter.ino   Bob Larkin 2 Feb 2022
 * Measure frequency response of LPF.
 *
 * This uses the Goertzel algoritm of AudioAnalyzeToneDetect_F32 as
 * a "direct conversion receiver."  This provides the excellent
 * selectivity needed for measuring below -100 dBfs.
 *
 *  Public Domain - Teensy
 */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"

AudioSynthSineCosine_F32 sine_cos1;          //xy=87,151
AudioFilterConvolution_F32 convolution1;     //xy=163,245
AudioFilterFIRGeneral_F32 filterFIRgeneral1; //xy=285,177
AudioOutputI2S_F32       audioOutI2S1;       //xy=357,337
AudioAnalyzeToneDetect_F32 toneDet1;         //xy=377,259
AudioAnalyzeToneDetect_F32 toneDet2;         //xy=378,299
AudioConnection_F32          patchCord1(sine_cos1, 0, filterFIRgeneral1, 0);
AudioConnection_F32          patchCord2(sine_cos1, 1, convolution1, 0);
AudioConnection_F32          patchCord3(convolution1, toneDet2);
AudioConnection_F32          patchCord4(convolution1, 0, audioOutI2S1, 0);
AudioConnection_F32          patchCord5(filterFIRgeneral1, 0, toneDet1, 0);
AudioControlSGTL5000     sgtl5000_1;         //xy=164,343

float32_t saveDat[512];
float32_t coeffFIR[512];
float32_t stateFIR[1160];
float32_t attenFIR[256];
float32_t rdb[1000];

void setup(void) {
  AudioMemory(5);
  AudioMemory_F32(50);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test Convolutional Filtering routine for F32 ***");
  sine_cos1.frequency(1000.0f);
  sine_cos1.amplitude(1.0f);
  toneDet1.frequency(100.0f, 5);
  toneDet2.frequency(100.0f, 5);
  convolution1.initFilter(4000.0f, 60.0f, 0, 0.0f);
  convolution1.begin(1);

  // Design for direct FIR.  This sets frequency response.
  // Bin for 4kHz at 44.117kHz sample rate and 400 coeff's is (4000/44117) * (512/2) = 23.2
  for(int ii=0; ii<47; ii++)    attenFIR[ii] = 0.0f;
  for(int ii=47; ii<256; ii++)  attenFIR[ii] = -150.0f;
  // FIRGeneralNew(float *adb, uint16_t nFIR, float *cf32f, float kdb, float *pStateArray);
  filterFIRgeneral1.FIRGeneralNew(attenFIR, 512, coeffFIR, 60.0, stateFIR);

  // DSP measure frequency response in dB  uniformly spaced from 100 to 20000 Hz
  Serial.println("Freq Hz  Conv dB  FIR dB");
  for(float32_t f=100.0; f<=20000.0f; f+=50.0f)
     {
     sine_cos1.frequency(f);
     toneDet1.frequency(f, (f>2000.0f ? 100 : 5));
     toneDet2.frequency(f, (f>2000.0f ? 100 : 5));
     delay(50);
     while(!toneDet1.available())delay(2) ;
     toneDet1.read();
     while(!toneDet2.available())delay(2) ;
     toneDet2.read();
     while(!toneDet2.available())delay(2) ;
     Serial.print(f);
     Serial.print(", ");
     while(!toneDet2.available())delay(2) ;
     Serial.print(20.0f*log10f(toneDet2.read() ) );
     Serial.print(", ");
     while(!toneDet1.available())delay(2) ;
     Serial.println(20.0f*log10f(toneDet1.read() ) );
     }
  }

void loop() {
}
