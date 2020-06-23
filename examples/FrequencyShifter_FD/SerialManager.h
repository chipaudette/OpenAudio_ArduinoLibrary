

#ifndef _SerialManager_h
#define _SerialManager_h

#include <Tympan_Library.h>


//now, define the Serial Manager class
class SerialManager {
  public:
  public:
    SerialManager(Tympan &_audioHardware)
      : audioHardware(_audioHardware)
    {  };
    //SerialManager(void)
    //{  };
          
    void respondToByte(char c);
    void printHelp(void);
    int N_CHAN;
    float channelGainIncrement_dB = 2.5f; 
    int freq_shift_increment = 1;
    
  private:
    Tympan &audioHardware;
};
#define thisSerial audioHardware

void SerialManager::printHelp(void) {  
  thisSerial.println();
  thisSerial.println("SerialManager Help: Available Commands:");
  thisSerial.println("   h: Print this help");
  thisSerial.println("   g: Print the gain settings of the device.");
  thisSerial.println("   C: Toggle printing of CPU and Memory usage");
  thisSerial.println("   p: Switch to built-in PCB microphones");
  thisSerial.println("   m: switch to external mic via mic jack");
  thisSerial.println("   l: switch to line-in via mic jack");
  thisSerial.print("   k: Increase the gain of all channels (ie, knob gain) by "); thisSerial.print(channelGainIncrement_dB); thisSerial.println(" dB");
  thisSerial.print("   K: Decrease the gain of all channels (ie, knob gain) by ");thisSerial.print(-channelGainIncrement_dB); thisSerial.println(" dB");
  thisSerial.print("   f: Raise freq shifting (change by "); thisSerial.print(freq_shift_increment); thisSerial.println(" bins)");
  thisSerial.print("   F: Lower freq shifting (change by "); thisSerial.print(-freq_shift_increment); thisSerial.println(" bins)");  thisSerial.println();
}

//functions in the main sketch that I want to call from here
extern void incrementKnobGain(float);
extern void printGainSettings(void);
extern void togglePrintMemoryAndCPU(void);
extern int incrementFreqShift(int);
extern void switchToPCBMics(void);
extern void switchToMicInOnMicJack(void);
extern void switchToLineInOnMicJack(void);

//switch yard to determine the desired action
void SerialManager::respondToByte(char c) {
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
      thisSerial.println("Received: toggle printing of memory and CPU usage.");
      togglePrintMemoryAndCPU(); break;
    case 'p':
      switchToPCBMics(); break;
    case 'm':
      switchToMicInOnMicJack(); break;
    case 'l':
      switchToLineInOnMicJack();break;
    case 'f':
      { int new_val = incrementFreqShift(freq_shift_increment);
      thisSerial.print("Recieved: new freq shift = "); thisSerial.println(new_val);}
      break;
    case 'F':
      { int new_val = incrementFreqShift(-freq_shift_increment);
      thisSerial.print("Recieved: new freq shift = "); thisSerial.println(new_val);}
      break;
  }
}


#endif
