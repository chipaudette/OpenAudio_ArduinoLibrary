/*
* CWSendTest4Rate.ino  Test the
* F32 library CW Modulator at four sample rates.
* Bob Larkin 29 August 2025
* Public Domain
*/

// This is a test of the CW Modulator
// Runs at 12, 24, 48 and 96 ksps.  Sends a test string out
// to both Left and Right channels.

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include <Audio.h>
// Next include has set_audioClock(  )
#include <utility/imxrt_hw.h> 

radioCWModulator_F32     CWMod1;         //xy=414,332
AudioOutputI2S_F32       audioOutI2S1;   //xy=595,327
AudioConnection_F32      patchCord1(CWMod1, 0, audioOutI2S1, 0);
AudioConnection_F32      patchCord2(CWMod1, 0, audioOutI2S1, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=509,390

char test12Str[] = "test 12  ";
char test24Str[] = "test 24  ";
char test48Str[] = "test 48  ";
char test96Str[] = "test 96  ";

void setup(void)
   {
   Serial.begin(9600);
   delay(1000);
   AudioMemory_F32(20);
   Serial.println("Test CW Transmit");

   setI2SFreq(12000);                  // Sets I2S clocks

   CWMod1.setSampleRate_Hz(12000.0f);  // Sets CW functions, only
   CWMod1.setCWSpeedWPM(13);
   CWMod1.setFrequency(700.0f);
   CWMod1.amplitude(0.3f);  // <== IMPORTANT Set before using headphones
   CWMod1.sendStringCW(test12Str);
   CWMod1.enableTransmit(true);
   Serial.println("Going to 12 ksps Sample Rate");
   while(CWMod1.getBufferSpace() < 512) delay(1);  // Send at 12 ksps

   setI2SFreq(24000);
   CWMod1.setSampleRate_Hz(24000.0f);
   CWMod1.sendStringCW(test24Str);
   Serial.println("Going to 24 ksps Sample Rate");
   while(CWMod1.getBufferSpace() < 512) delay(1);   // Send at 24 ksps

   setI2SFreq(48000);
   CWMod1.setSampleRate_Hz(48000.0f);
   CWMod1.sendStringCW(test48Str);
   Serial.println("Going to 48 ksps Sample Rate");
   while(CWMod1.getBufferSpace() < 512) delay(1);   // Send at 48 ksps

   setI2SFreq(96000);
   CWMod1.setSampleRate_Hz(96000.0f);
   CWMod1.sendStringCW(test96Str);
   Serial.println("Going to 96 ksps Sample Rate");
   while(CWMod1.getBufferSpace() < 512) delay(1);   // Send at 96 ksps
   delay(5);

   Serial.println("End of Transmission");
   }

void loop(void)
   {
   delay(1);
   }

// Frank B's routine for setting I2S clocks. *** FOR T4.x ONLY ***
void setI2SFreq(int freq1)    // Thank ypu, Frank B.
   {
   // PLL between 27*24 = 648MHz und 54*24=1296MHz
   int n1 = 4; //SAI prescaler 4 => (n1*n2) = multiple of 4
   int n2 = 1 + (24000000 * 27) / (freq1 * 256 * n1);
   double C = ((double)freq1 * 256 * n1 * n2) / 24000000;
   int c0 = C;
   int c2 = 10000;
   int c1 = C * c2 - (c0 * c2);
   set_audioClock(c0, c1, c2, true);
   CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
       | CCM_CS1CDR_SAI1_CLK_PRED(n1-1)                                // &0x07
       | CCM_CS1CDR_SAI1_CLK_PODF(n2-1);                               // &0x3f 
   }
