//
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
// T4.0: Uses 7.8% processor and 9 F32 memory blocks, both max.
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

float saveDat[512];

void setup() {
  Serial.begin(300); // Any speed works
  delay(1000);

  AudioMemory_F32(50);

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

  Serial.println("1024 point real FFT output in dB relative to full scale sine wave");
  }

void loop() {
  static uint32_t nTimes = 0;
  if ( myFFT.available() ) {
     // each time new FFT data is available
     // print it all to the Arduino Serial Monitor
     float* pin = myFFT.getData();
     for (int  kk=0; kk<512; kk++)
        saveDat[kk]= *(pin + kk);
     if(++nTimes>4 && nTimes<6)  {
         for (int i=0; i<512; i++) {
            Serial.print(i);
            Serial.print(",  ");
            Serial.println(saveDat[i], 8);
            }
         Serial.println();
         Serial.print("CPU: Max Percent Usage: ");
         Serial.println(AudioProcessorUsageMax());
         Serial.print("   Max Float 32 Memory: ");
         Serial.println(AudioMemoryUsageMax_F32());
         }
     }
  }

