/* TestOutput_float.ino  Bob Larkin 3 July 2020
 * 
 * Test for #define USE_F32_IO either 0 or 1, all OK on T3.6
 * 
 */
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h> 

// NOT WORKING for USE_F32_IO 1  <<<<<<<<<<<<<<<
#define ALL_TEENSY_AUDIO 0
#define USE_F32_IO 1

#if ALL_TEENSY_AUDIO

AudioSynthWaveformSine sine1;
AudioOutputI2S         i2sOut;
AudioConnection        patchCord1(sine1, 0, i2sOut, 0);
AudioConnection        patchCord2(sine1, 0, i2sOut, 1);
AudioControlSGTL5000   sgtl5000_1;

void setup(void) {
  Serial.begin(1);   delay(1000);
  Serial.println("Teensy Audio, No F32"); 
 
  AudioMemory(10);
  sgtl5000_1.enable();
  sine1.frequency(300.0);
  sine1.amplitude(0.005f);
} 

void loop() {
}
// ================================================

#else //  OpenAudio F32
#if USE_F32_IO
AudioSynthWaveformSine_F32 sine1;
AudioOutputI2S_OA_F32      i2sOut;
AudioConnection_F32        patchCord1(sine1, 0, i2sOut, 0);
AudioConnection_F32        patchCord2(sine1, 0, i2sOut, 1);

#else    // Use F32toI16 convert and I16 out
AudioSynthWaveformSine_F32 sine1;
AudioConvert_F32toI16      float2Int1, float2Int2;
AudioOutputI2S             i2sOut;
AudioConnection_F32        patchCord5(sine1,      0, float2Int1, 0);
AudioConnection_F32        patchCord6(sine1,      0, float2Int2, 0);
AudioConnection            patchCord7(float2Int1, 0, i2sOut,     0);
AudioConnection            patchCord8(float2Int2, 0, i2sOut,     1);
#endif
AudioControlSGTL5000       sgtl5000_1;

void setup(void) {
  Serial.begin(1);   delay(1000);
#if USE_F32_IO
  Serial.println("Open Audio: Test direct F32 Output"); 
#else
  Serial.println("Open Audio: Test Convert to Teensy Audio I16  Output");
#endif 
  AudioMemory(10);
  AudioMemory_F32(10);

//delay(1); Serial.println("Start i2s_f32 out");
//i2sOut.begin();
//delay(1); Serial.println("Start codec");

  sgtl5000_1.enable();
  
  sine1.frequency(300.0);
  sine1.amplitude(0.005f);
  sine1.begin();
} 

void loop() {
}

#endif  // ALL_TEENSY_AUDIO
