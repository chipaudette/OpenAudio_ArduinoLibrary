/*
   SineCosOut.ino  - Sine left, Cos right
   Bob Larkin 28 Dec 2020

   Basic test of I2S input and output.
   Tested on T3.6 and T4.0
   Public Domain
 */
#include "Audio.h"       // Teensy I16 Audio Library
#include "OpenAudio_ArduinoLibrary.h" // F32 library
#include "AudioStream_F32.h"

AudioSynthSineCosine_F32    sineCos;
AudioAnalyzePeak_F32        peakL;
AudioAnalyzePeak_F32        peakR;
//AudioOutputI2S_OA_F32       i2sOut;
AudioOutputI2S_F32       i2sOut;
AudioConnection_F32         patchCord0(sineCos,   0, peakL,  0);
AudioConnection_F32         patchCord1(sineCos,   1, peakR,  0);
AudioConnection_F32         patchCord2(sineCos,   0, i2sOut, 0);  // Sine
AudioConnection_F32         patchCord3(sineCos,   1, i2sOut, 1);  // Cosine
AudioControlSGTL5000        codec1;

void setup() {
  Serial.begin(9600); // (anything)
  delay(1000); 
  Serial.println("OpenAudio_ArduinoLibrary - Sine-Cosine Stereo");
  // Internally, I16 memory blocks are used.  Needs modification, but
  // for now, supply 4 for F32 input and 4 for F32 output (shared).
  AudioMemory(4);
  AudioMemory_F32(20);
  codec1.enable();  // MUST be before inputSelect()
  codec1.inputSelect(AUDIO_INPUT_LINEIN);
  sineCos.amplitude(0.2); sineCos.frequency(600.0f);
}

void loop() {
    Serial.print("Max float memory = ");
    Serial.println(AudioStream_F32::f32_memory_used_max);
    if(peakL.available())  Serial.print(peakL.read(), 6);
    Serial.print(" <-L   R-> ");
    if(peakR.available())  Serial.println(peakR.read(), 6);
    delay(500);
}
