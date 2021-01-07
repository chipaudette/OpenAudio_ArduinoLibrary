/*
   SineCosOut.ino  - Sine left, Cos right
   Selectable:  Frequency, Hz
                Amplitude, 0.00 to 1.00
   Bob Larkin 28 Dec 2020

   Basic test of I2S input and output.
   Tested on T3.6 and T4.0
   Public Domain
 */
#include "Audio.h"       // Teensy I16 Audio Library
#include "OpenAudio_ArduinoLibrary.h" // F32 library
#include "AudioStream_F32.h"
 
// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will hanedle.
const float sample_rate_Hz = 24000.0f ;  // 24000, 44117, or other frequencies listed above
const int   audio_block_samples = 128;    // Always 128, which is AUDIO_BLOCK_SAMPLES from AudioStream.h
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

AudioSynthSineCosine_F32    sineCos;
AudioAnalyzePeak_F32        peakL;
AudioAnalyzePeak_F32        peakR;
AudioOutputI2S_F32          i2sOut(audio_settings);
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
  //AudioMemory(4);
  AudioMemory_F32(20);
  codec1.enable();  // MUST be before inputSelect()
  codec1.inputSelect(AUDIO_INPUT_LINEIN);
  float fr = 600.0f; //  Sine wave frequency and correction for sample rate
  sineCos.amplitude(0.2); sineCos.frequency(fr*44117.647f/sample_rate_Hz);
}

void loop() {
    Serial.print("Max float memory = ");
    Serial.println(AudioStream_F32::f32_memory_used_max);
    if(peakL.available())  Serial.print(peakL.read(), 6);
    Serial.print(" <-L  Peak level  R-> ");
    if(peakR.available())  Serial.println(peakR.read(), 6);
    delay(500);
}
