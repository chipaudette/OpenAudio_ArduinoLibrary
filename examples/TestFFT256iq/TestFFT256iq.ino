
// TestFFT256iq.ino

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioSynthSineCosine_F32 sine_cos1;      //xy=76,532
AudioAnalyzeFFT256_IQ_F32 FFT256iq1;      //xy=243,532
AudioOutputI2S_F32       audioOutI2S1;   //xy=246,591
AudioConnection_F32      patchCord1(sine_cos1, 0, FFT256iq1, 0);
AudioConnection_F32      patchCord2(sine_cos1, 1, FFT256iq1, 1);
//AudioControlSGTL5000     sgtl5000_1;
// GUItool: end automatically generated code

void setup(void) {
  float* pPwr;

  Serial.begin(9600);
  delay(1000);
  AudioMemory_F32(20);
  Serial.println("FFT256IQ Test");
//  sgtl5000_1.enable();                   //start the audio board
 // sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);       // or AUDIO_INPUT_MIC

  sine_cos1.amplitude(0.5);         // Initialize Waveform Generator
  // bin spacing = 44117.648/256 = 172.335   172.3 * 4 = 689.335 Hz (T3.6)
  // Half bin higher is 775.3 for testing windows
  //sine_cos1.frequency(689.34f);
  sine_cos1.frequency(1723.35f);

  FFT256iq1.setOutputType(FFT_DBFS);
  FFT256iq1.windowFunction(AudioWindowBlackmanHarris256);
  //float* pw = FFT256iq1.getWindow();   // Print window
  //for (int i=0; i<256; i++) Serial.println(pw[i], 4);

  delay(1000);
  if( FFT256iq1.available() )
      pPwr = FFT256iq1.getData();

  for(int i=0; i<256; i++)
     Serial.println(*(pPwr + i), 8 );
  }

void loop(void)  {
  }
