
// TestFFT1024iq.ino  for Teensy 3.x, 4.x
// Bob Larkin 3 March 2021// Rev to include xAxis control. 6 Mar 2021

// Generate Sin and Cosine pair and input to IQ FFT.
// Serial Print out powers of all 1024 bins in
// dB relative to Sine Wave Full Scale

// Public Domain

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"

// GUItool: begin automatically generated code
AudioSynthSineCosine_F32   sine_cos1;       //xy=76,532
AudioAnalyzeFFT1024_IQ_F32 FFT1024iq1;      //xy=243,532
AudioOutputI2S_F32         audioOutI2S1;    //xy=246,591
AudioConnection_F32        patchCord1(sine_cos1, 0, FFT1024iq1, 0);
AudioConnection_F32        patchCord2(sine_cos1, 1, FFT1024iq1, 1);
// GUItool: end automatically generated code

void setup(void) {
  float* pPwr;

  Serial.begin(9600);
  delay(1000);
  AudioMemory_F32(50);
  Serial.println("FFT1024IQ Test");

  sine_cos1.amplitude(1.0f); // Initialize Waveform Generator

  // Pick T3.6 bin center
  //sine_cos1.frequency(689.33);

  // or pick T4.x bin center
  sine_cos1.frequency(689.0625f);

  // or pick any old frequency
  //sine_cos1.frequency(7100.0);

  // elect the output format
  FFT1024iq1.setOutputType(FFT_DBFS);

  // Select the wndow function
  //FFT1024iq1.windowFunction(AudioWindowNone);
  //FFT1024iq1.windowFunction(AudioWindowHanning1024);
  //FFT1024iq1.windowFunction(AudioWindowKaiser1024, 55.0f);
  FFT1024iq1.windowFunction(AudioWindowBlackmanHarris1024);

  // Uncomment to Serial print window function
  //float* pw = FFT1024iq1.getWindow();   // Print window
  //for (int i=0; i<512; i++) Serial.println(pw[i], 4);

  FFT1024iq1.setNAverage(4);

  // xAxis, bit 0 left/right;  bit 1 low to high;  default 0X03
  FFT1024iq1.setXAxis(0X02);

  delay(1000);
  // Print output, once
  if( FFT1024iq1.available() )  {
      pPwr = FFT1024iq1.getData();
      for(int i=0; i<1024; i++)
        Serial.println(*(pPwr + i), 8 );
      }
  Serial.println("");
  }

void loop(void)  {
  }
