
// TestFFT256iq.ino

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioSynthSineCosine_F32  sine_cos1;      //xy=76,532
AudioAnalyzeFFT256_IQ_F32 FFT256iq1;      //xy=243,532
AudioOutputI2S_F32        audioOutI2S1;   //xy=246,591
AudioConnection_F32       patchCord1(sine_cos1, 0, FFT256iq1, 0);
AudioConnection_F32       patchCord2(sine_cos1, 1, FFT256iq1, 1);
// GUItool: end automatically generated code

void setup(void) {
  float* pPwr;

  Serial.begin(9600);
  delay(1000);
  AudioMemory_F32(20);
  Serial.println("FFT256IQ Test   v2");

  sine_cos1.amplitude(1.0);         // Initialize Waveform Generator
  // bin spacing = 44117.648/256 = 172.335   172.3 * 4 = 689.335 Hz (T3.6)
  // Half bin higher is 775.3 for testing windows
  //sine_cos1.frequency(689.34f);

  // Pick T3.6 bin center
  //sine_cos1.frequency(689.33);

  // or pick T4.x bin center
  //sine_cos1.frequency(689.0625f);

  // or pick any old frequency
  sine_cos1.frequency(7100.0);

  // elect the output format
  FFT256iq1.setOutputType(FFT_DBFS);

  // Select the wndow function
  //FFT256iq1.windowFunction(AudioWindowNone);
  //FFT256iq1.windowFunction(AudioWindowHanning256);
  //FFT256iq1.windowFunction(AudioWindowKaiser256, 55.0f);
  FFT256iq1.windowFunction(AudioWindowBlackmanHarris256);

  // Uncomment to Serial print window function
  //float* pw = FFT256iq1.getWindow();   // Print window
  //for (int i=0; i<512; i++) Serial.println(pw[i], 4);

  // xAxis, bit 0 left/right;  bit 1 low to high;  default 0X03
  FFT256iq1.setXAxis(0X03);

  FFT256iq1.windowFunction(AudioWindowBlackmanHarris256);
  //float* pw = FFT256iq1.getWindow();   // Print window
  //for (int i=0; i<256; i++) Serial.println(pw[i], 4);

  // Do power averaging (outputs appear less often, as well)
  FFT256iq1.setNAverage(5);  // nAverage >= 1

  delay(1000);
  if( FFT256iq1.available() )  {
     pPwr = FFT256iq1.getData();
     for(int i=0; i<256; i++) {
         Serial.print(i);
         Serial.print(",");
         Serial.println(*(pPwr + i), 8 );
         }
      Serial.print("\n\n");
      }
  }

void loop(void)  {
  }
