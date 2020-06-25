// SerialManager_FreqShift_OA.h
// Demonstrate frequency shifting via frequency domain processing.
//
// Created: Chip Audette (OpenAudio) Aug 2019
// Built for the Tympan library for Teensy 3.6-based hardware
//
// Convert to Open Audio Bob Larkin June 2020
//
// MIT License.  Use at your own risk.
//

#ifndef _SerialManagerFreqShift_OA_h
#define _SerialManagerFreqShift_OA_h
#include "AudioStream_F32.h"
#include "OpenAudio_ArduinoLibrary.h"
//now, define the Serial Manager class
class SerialManagerFreqShift_OA {
  public:
    SerialManagerFreqShift_OA(void)  {  };
          
    void respondToByte(char c);
    void printHelp(void);
    int N_CHAN;
    float channelGainIncrement_dB = 2.5f; 
    int freq_shift_increment = 1;
 };

void SerialManagerFreqShift_OA::printHelp(void) {  
  Serial.println();
  Serial.println("SerialManager Help: Available Commands:");
  Serial.println("   h: Print this help");
  Serial.println("   g: Print the gain settings of the device.");
  Serial.println("   C: Toggle printing of CPU and Memory usage");
  Serial.print(  "   k: Increase the gain of all channels (ie, knob gain) by ");
         Serial.print(channelGainIncrement_dB); Serial.println(" dB");
  Serial.print(  "   K: Decrease the gain of all channels (ie, knob gain) by ");
         Serial.print(-channelGainIncrement_dB); Serial.println(" dB");
  Serial.print(  "   f: Raise freq shifting (change by "); Serial.print(freq_shift_increment); Serial.println(" bins)");
  Serial.print(  "   F: Lower freq shifting (change by "); Serial.print(-freq_shift_increment); Serial.println(" bins)");  Serial.println();
}

//functions in the main sketch that I want to call from here
extern void incrementKnobGain(float);
extern void printGainSettings(void);
extern void togglePrintMemoryAndCPU(void);
extern int incrementFreqShift(int);

//switch yard to determine the desired action
void SerialManagerFreqShift_OA::respondToByte(char c) {
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
      { int new_val = incrementFreqShift(freq_shift_increment);
      Serial.print("Recieved: new freq shift = "); Serial.println(new_val);}
      break;
    case 'F':
      { int new_val = incrementFreqShift(-freq_shift_increment);
      Serial.print("Recieved: new freq shift = "); Serial.println(new_val);}
      break;
  }
}
#endif
