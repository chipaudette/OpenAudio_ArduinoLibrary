/* AM_PM_FM.ino     Bob Larkin 20 April 2021
 * 
 * This tests the radioModulatedGenerator class in AM and
 * FM for the simple case of sine wave modulation.  The "carrier
 * is 15000 Hz and a single output is produced.  This is only
 * a partial test of the class as it supports phase modulation and
 * formats such as QAM.
* 
* Public Domain
 */

#include "AudioStream_F32.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "Arduino.h"
#include "Audio.h"

// *****************    CONTROLS   **********************
// Comment out one of the next two:
#define DO_AM
//#define DO_FM

// *******************************************************

// Use SineCosine as it allows amplitudes greater than 
// 1.0 (this is FP and that is OK). Use 0 channnel only.
AudioSynthSineCosine_F32        sine1;
//AudioSynthWaveformSine_F32    sine1;  // Restricts amplitude
radioModulatedGenerator_F32   modGen1;
AudioOutputI2S_F32            i2sOut1;

#ifdef DO_AM
AudioConnection_F32    pcord0(sine1,   0, modGen1, 0);
#endif
#ifdef DO_FM
AudioConnection_F32    pcord0(sine1,   0, modGen1, 1);
#endif

AudioConnection_F32    pcord1(modGen1, 0, i2sOut1, 0);
AudioControlSGTL5000   codec1;

void setup(void) {
  AudioMemory_F32(25);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test Modulated Generator ***"); delay(2000);
  codec1.enable();  // MUST be before inputSelect()
  codec1.inputSelect(AUDIO_INPUT_LINEIN);

  sine1.frequency(567.0f);       // Modulation frequency
  modGen1.frequency(15000.0f);   // Carrier frequency
#ifdef DO_AM
  sine1.amplitude(1.0f);    // 100% AM modulation
  modGen1.amplitude(0.25);  // Total including AM peaks less than +/-1.0
  modGen1.doModulation_AM_PM_FM(true, false, false, false);  //AM, single output
#endif

#ifdef DO_FM
  sine1.amplitude(3000.0f); // 3 kHz deviation
  modGen1.amplitude(1.0);  // Max DAC level
  modGen1.doModulation_AM_PM_FM(false, false, true, false);  //FM, single output
#endif
}

void loop(void) {
}
