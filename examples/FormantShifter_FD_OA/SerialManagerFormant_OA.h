/*
 * SerialManagerFormant_OA.h
 * Demonstrate formant shifting via frequency domain processing
 * Created: Chip Audette (OpenAudio) March 2019
 *
 * Moved to OpenAudio, removed Tympan dependencies, fixed for T4.x
 * Bob Larkin June 2020
 *
 * MIT License.  Use at your own risk.
 */

#ifndef _SerialManagerFormant_OA_h
#define _SerialManagerFormant_OA_h

#include <OpenAudio_ArduinoLibrary.h>

//now, define the Serial Manager class
class SerialManagerFormant_OA {
  public:
     void respondToByte(char c);
     void printHelp(void);
     int N_CHAN;
     float channelGainIncrement_dB = 2.5f;
     float formantScaleIncrement = powf(2.0,1.0/6.0);
};

void SerialManagerFormant_OA::printHelp(void) {
  Serial.println();
  Serial.println("SerialManager_OA Help: Available Commands:");
  Serial.println("   h: Print this help");
  Serial.println("   g: Print the gain settings of the device.");
  Serial.println("   C: Toggle printing of CPU and Memory usage");
  Serial.print("   k: Increase the audio by ");
     Serial.print(channelGainIncrement_dB);
     Serial.println(" dB");
  Serial.print("   K: Decrease the audio gain by ");
     Serial.print(-channelGainIncrement_dB);
     Serial.println(" dB");
  Serial.print("   f: Raise formant shifting (change by ");
     Serial.print(formantScaleIncrement);
     Serial.println("x)");
  Serial.print("   F: Lower formant shifting (change by ");
     Serial.print(1.0/formantScaleIncrement);
     Serial.println("x)");
     Serial.println();
}

//functions in the main sketch that I want to call from here
extern void incrementKnobGain(float);
extern void printGainSettings(void);
extern void togglePrintMemoryAndCPU(void);
extern float incrementFormantShift(float);
//extern void switchToPCBMics(void);
//extern void switchToMicInOnMicJack(void);
//extern void switchToLineInOnMicJack(void);

//switch yard to determine the desired action
void SerialManagerFormant_OA::respondToByte(char c) {
  //float old_val = 0.0, new_val = 0.0;
  switch (c) {
    case 'h': case '?':
      printHelp(); break;
    case 'g': case 'G':
      printGainSettings(); break;
    case 'k':
      incrementKnobGain(channelGainIncrement_dB); break;
    case 'K':   //which is "shift k"
      incrementKnobGain(-channelGainIncrement_dB);  break;
    case 'C': case 'c':
      Serial.println("Received: toggle printing of memory and CPU usage.");
      togglePrintMemoryAndCPU(); break;
    case 'f':
      { float new_val = incrementFormantShift(formantScaleIncrement);
      Serial.print("Recieved: new formant scale = ");  Serial.println(new_val);}
      break;
    case 'F':
      { float new_val = incrementFormantShift(1./formantScaleIncrement);
      Serial.print("Recieved: new formant scale = ");  Serial.println(new_val);}
      break;
  }
}
#endif
