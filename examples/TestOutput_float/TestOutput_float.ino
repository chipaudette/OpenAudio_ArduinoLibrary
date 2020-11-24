/* TestOutput_float.ino  Bob Larkin 3 July 2020
 * 
 */
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h> 

AudioSynthWaveformSine_F32 sine1;
AudioOutputI2S_OA_F32      i2sOut;
AudioConnection_F32        patchCord1(sine1, 0, i2sOut, 0);
AudioControlSGTL5000       sgtl5000_1;

void setup(void) {
  Serial.begin(9600);   delay(1000);
  Serial.println("Open Audio: Test direct F32 Output"); 
  AudioMemory(10);
  AudioMemory_F32(10);

  // Next line, moved from constructor, allows Loader to operate without crash
                   i2sOut.begin();  // Uncomment here and comment out in constructor for AudioOutputI2S_OA_F32
  
  sgtl5000_1.enable();
  
  sine1.frequency(300.0);  sine1.amplitude(0.005f);
  sine1.begin();
} 

void loop() {
}
