/*
   PassthroughF32.ino  - Stereo passthrough.
   Bob Larkin 22 Dec 2020

   Basic test of I2S input and output.
   Tested T3.6 and     5 Jan 2021
   Public Domain 
 */
#include "Arduino.h"
#include "Audio.h"       // Teensy I16 Audio Library
#include "OpenAudio_ArduinoLibrary.h" // F32 library
#include "AudioStream_F32.h"
/*
#include "Audio.h"       // Teensy I16 Audio Library
#include "OpenAudio_ArduinoLibrary.h" // F32 library
#include "AudioStream_F32.h"  */

// AudioInputI2S_OA_F32        i2sIn;
AudioInputI2S_F32        i2sIn;
AudioAnalyzePeak_F32        peakL;
AudioAnalyzePeak_F32        peakR;
// AudioOutputI2S_OA_F32       i2sOut;
AudioOutputI2S_F32       i2sOut;

AudioConnection_F32         patchCord0(i2sIn,   0, peakL,  0);
AudioConnection_F32         patchCord1(i2sIn,   1, peakR,  0);
AudioConnection_F32         patchCord2(i2sIn,   0, i2sOut, 0);
AudioConnection_F32         patchCord3(i2sIn,   1, i2sOut, 1);
AudioControlSGTL5000        codec1;

void setup() {
  Serial.begin(9600); // (anything)
  delay(1000); 
  Serial.println("OpenAudio_ArduinoLibrary - Passthrough Stereo");

 // Internally, I16 blocks are used  Bob Fix sometime
 // AudioMemory(4);  // Needed or I/O
  AudioMemory_F32(20);
  codec1.enable();  // MUST be before inputSelect()
  codec1.inputSelect(AUDIO_INPUT_LINEIN);
}

void loop() {
    Serial.print("Max float memory = ");
    Serial.println(AudioStream_F32::f32_memory_used_max);
    if(peakL.available())  Serial.print(peakL.read(), 6);
    Serial.print(" <-L   R-> ");
    if(peakR.available())  Serial.println(peakR.read(), 6);
    delay(1000);
}
