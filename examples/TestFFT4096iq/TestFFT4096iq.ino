
// TestFFT2048iq.ino  for Teensy 4.x
// Bob Larkin 9 March 2021

// Generate Sin and Cosine pair and input to IQ FFT.
// Serial Print out powers of all 4096 bins in
// dB relative to Sine Wave Full Scale

// Rev- added i to print; added filters on oscillators. 23 Feb 2022

// Public Domain

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"

int jj = 0;

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
  Serial.println("FFT4096IQ Test 2");

  sine_cos1.amplitude(1.0f); // Initialize Waveform Generator

  // Pick T4.x bin center
  //sine_cos1.frequency(689.0625f);

  // or pick any old frequency
  sine_cos1.frequency(1000.0f);

  // Engage the identical BP Filters on sine/cosine outputs (true).
  sine_cos1.pureSpectrum(true);

  // Select the output format  FFT_POWER, FFT_RMS or FFT_DBFS
  FFT4096iq1.setOutputType(FFT_DBFS);

  // Select the wndow function from these four:
  //FFT4096iq1.windowFunction(AudioWindowNone);
  //FFT4096iq1.windowFunction(AudioWindowHanning4096);
  //FFT4096iq1.windowFunction(AudioWindowKaiser4096, 55.0f);
  FFT4096iq1.windowFunction(AudioWindowBlackmanHarris4096);

  // Uncomment to Serial print window function
  // float* pw = FFT4096iq1.getWindow();   // Print window
  // for (int i=0; i<4096; i++) Serial.println(pw[i], 7);

  // x-Axis direction and offset for sine to I and cosine to Q.
  //   If xAxis=0  f=fs/2 in middle, f=0 on right edge
  //   If xAxis=1  f=fs/2 in middle, f=0 on left edge
  //   If xAxis=2  f=fs/2 on left edge, f=0 in middle
  //   If xAxis=3  f=fs/2 on right edge, f=0 in middle
  // xAxis=3 is mathematically proper -fs/2 start to +fs/2 stop.
  FFT4096iq1.setXAxis(0X03);

  FFT4096iq1.setNAverage(5);
  }

void loop(void)  {
  static bool doPrint=true;
  float *pPwr;
  // Print output, once
  if( FFT4096iq1.available() && jj++>2 && doPrint )
      {
      pPwr = FFT4096iq1.getData();
      for(int i=0; i<4096; i++)
        {
        // Serial.print((int)((float32_t)i * 44100.0/4096.0)); // Print freq
        Serial.print(i);     // FFT Output index (0, 4095)
        Serial.print(" ");
        Serial.println(*(pPwr + i), 8 );
	    }
      doPrint = false;
      }
  if(doPrint)
      {
      Serial.print(" Audio MEM Float32 Peak: ");
      Serial.println(AudioMemoryUsageMax_F32());
      }
  delay(100);
  }
