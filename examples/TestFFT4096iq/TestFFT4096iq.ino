
// TestFFT2048iq.ino  for Teensy 4.x
// Bob Larkin 9 March 2021

// Generate Sin and Cosine pair and input to IQ FFT.
// Serial Print out powers of all 4096 bins in
// dB relative to Sine Wave Full Scale

// Public Domain

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"

// GUItool: begin automatically generated code
AudioSynthSineCosine_F32   sine_cos1;       //xy=76,532
AudioAnalyzeFFT4096_IQ_F32 FFT4096iq1;      //xy=243,532
AudioOutputI2S_F32         audioOutI2S1;    //xy=246,591
AudioConnection_F32        patchCord1(sine_cos1, 0, FFT4096iq1, 0);
AudioConnection_F32        patchCord2(sine_cos1, 1, FFT4096iq1, 1);
// GUItool: end automatically generated code

void setup(void) {

  Serial.begin(9600);
  delay(1000);

  // The 4096 complex FFT needs 32 F32 memory for real and 32 for imag.
  // Set memory to more than 64, depending on other useage.
  AudioMemory_F32(100);
  Serial.println("FFT4096IQ Test");

  sine_cos1.amplitude(1.0f); // Initialize Waveform Generator

  // Pick T4.x bin center
  //sine_cos1.frequency(689.0625f);

  // or pick any old frequency
  sine_cos1.frequency(1000.0f);

  // elect the output format
  FFT4096iq1.setOutputType(FFT_DBFS);

  // Select the wndow function
  //FFT4096iq1.windowFunction(AudioWindowNone);
  //FFT4096iq1.windowFunction(AudioWindowHanning4096);
  //FFT4096iq1.windowFunction(AudioWindowKaiser4096, 55.0f);
  FFT4096iq1.windowFunction(AudioWindowBlackmanHarris4096);

  // Uncomment to Serial print window function
  // float* pw = FFT4096iq1.getWindow();   // Print window
  // for (int i=0; i<4096; i++) Serial.println(pw[i], 7);

  // xAxis, bit 0 left/right;  bit 1 low to high;  default 0X03
  FFT4096iq1.setXAxis(0X03);

  FFT4096iq1.setNAverage(1);
  delay(100);
  }

void loop(void)  {
  static bool doPrint=true;
  float *pPwr;
  // Print output, once
  if( FFT4096iq1.available() && doPrint )  {
      pPwr = FFT4096iq1.getData();
      for(int i=0; i<4096; i++)
        Serial.println(*(pPwr + i), 8 );
      doPrint = false;
      }
  Serial.print(" Audio MEM Float32 Peak: ");
  Serial.println(AudioMemoryUsageMax_F32());
  delay(500);
  }
