#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include "OpenAudio_ArduinoLibrary.h"

// GUItool: begin automatically generated code
AudioSynthWaveformSine_F32  sine2;          //xy=179,339
AudioSynthWaveformSine_F32  sine1;          //xy=184,267
AudioOutputI2S2_F32         audio_out;   //xy=600,261
AudioConnection_F32         pc1(sine1, 0, audio_out, 0);
AudioConnection_F32         pc2(sine2, 0, audio_out, 1);
AudioControlSGTL5000        sgtl5000_1;     //xy=591,96
// GUItool: end automatically generated code

void setup(void) {

  AudioMemory(4);
  AudioMemory_F32(4);

  sgtl5000_1.enable();
  
  sine1.frequency(300);
  sine1.amplitude(0.5);
  sine1.begin();

  sine2.frequency(600);
  sine2.amplitude(0.8);
  sine2.begin();

} 

void loop() {}