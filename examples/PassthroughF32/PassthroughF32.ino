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

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will handle.
const float sample_rate_Hz = 24000.0f ;  // 24000, 44117, or other frequencies listed above
const int   audio_block_samples = 32;
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

AudioInputI2S_F32           i2sIn(audio_settings);
AudioAnalyzePeak_F32        peakL(audio_settings);
AudioAnalyzePeak_F32        peakR(audio_settings);
AudioOutputI2S_F32          i2sOut(audio_settings);

AudioConnection_F32         patchCord0(i2sIn,   0, peakL,  0);
AudioConnection_F32         patchCord1(i2sIn,   1, peakR,  0);
AudioConnection_F32         patchCord2(i2sIn,   0, i2sOut, 0);
AudioConnection_F32         patchCord3(i2sIn,   1, i2sOut, 1);
AudioControlSGTL5000        codec1;

void setup() {
  Serial.begin(9600); // (anything)
  delay(1000); 
  Serial.println("OpenAudio_ArduinoLibrary - Passthrough Stereo");

  AudioMemory_F32(20, audio_settings);
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
