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

const int myInput = AUDIO_INPUT_LINEIN;
//const int myInput = AUDIO_INPUT_MIC;

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

uint32_t ct = 0;
uint32_t count = 0;

void setup() {
  Serial.begin(300); // Any speed works
  delay(1000);

  AudioMemory_F32(20);

  // Enable the audio shield and set the output volume.
  audioShield.enable();
  audioShield.inputSelect(myInput);
  audioShield.volume(0.5);

  // Create a synthetic sine wave, for testing
  // To use this, edit the connections above
  // sinewave.frequency(1033.99f);   // Bin 24  T3.x
  // sinewave.frequency(1033.59375f);   // Bin 24  T4.x at 44100
  // sinewave.frequency(1055.0f);  // Bin 24.5, demonstrates windowing
  sinewave.frequency(1076.0f);

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
    Serial.println("FFT Output: ");
    for (int i=0; i<512; i++) {
        Serial.print(i);
        Serial.print(",");
        Serial.println(myFFT.read(i), 3);
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
  delay(500);
  }
