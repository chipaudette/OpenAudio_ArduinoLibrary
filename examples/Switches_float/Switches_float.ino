/* Switches_float.ino    Bob Larkin   20 June 2020
 *
 * Cascade an 4 position switch with an 8 position to swicth between
 * an RMS and a Peak measure of a sine wave.  This is a trivial
 * example of switching.  Normally, the switched paths would be involved
 * enough to benefit from saving computations and DC power.  An example
 * that is more like this is  <<<<<<<<<<<<<<<<<<<<<<<<<<
 *
 */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"

// To work with T4.0 the I2S routine outputs 16-bit integer (I16).  Then
// use Audette I16 to F32 convert.  Same below for output, in reverse.
AudioInputI2S in1;
AudioSynthWaveformSine_F32 sine1;
AudioSwitch4_OA_F32        switch1;
AudioSwitch8_OA_F32        switch2;
AudioAnalyzePeak_F32       peak1;
AudioAnalyzeRMS_F32        rms1;
AudioConnection_F32      connect1(sine1,     0, switch1, 0);
AudioConnection_F32      connect2(switch1,   2, switch2, 0);
AudioConnection_F32      connect3(switch2,   1, peak1,   0);
AudioConnection_F32      connect4(switch2,   6, rms1,    0);


void setup(void) {
  float32_t gain;
  AudioMemory(5);
  AudioMemory_F32(8);

  Serial.begin(1);  delay(1000);
  sine1.frequency(1000.0f);
  sine1.amplitude(0.2f);
  // switch1 is always set to channel 2 to feed switch2.
  switch1.setChannel(2);
}

void loop(void) {
  // Lets switch back and forth between RMS and Peak measurements.
  // This is switch2, channels 6 and 1.
  switch2.setChannel(6);   // RMS measurement
  delay(1000);
  if (rms1.available() )  {
	  Serial.print("RMS =");
	  Serial.println(rms1.read(), 6);
  }
  switch2.setChannel(1);   // Peak-to-peak measurement
  delay(1000);
  if (peak1.available() ) {
	  Serial.print("P-P =");
	  Serial.println(peak1.readPeakToPeak(), 6);}
  delay(1000);
}
