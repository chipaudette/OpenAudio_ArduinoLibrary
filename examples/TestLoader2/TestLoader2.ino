#include <Audio.h>

AudioSynthWaveformSine sine1;
AudioOutputI2S         i2sOut;
AudioConnection        patchCord1(sine1, 0, i2sOut, 0);
AudioConnection        patchCord2(sine1, 0, i2sOut, 1);
AudioControlSGTL5000   sgtl5000_1;

void setup(void) {
  Serial.begin(1);   delay(1000);
  Serial.println("Teensy Audio, Test Loader"); 
 
  AudioMemory(10);
  sgtl5000_1.enable();
  sine1.frequency(300.0);
  sine1.amplitude(0.005f);
} 

void loop() {
}
