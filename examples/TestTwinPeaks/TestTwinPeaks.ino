/* TestTwinPeaks.ino    Bob Larkin   26 Feb 2022    1
 * Tests the AlignLR class for finding the relative
 * time order of the Codec ADC L and R channels, and then
 * corrects these offsets.
 *
 * This applies a common analog square wave signal to the L and R inputs.
 * The cross-correlation between the channels is found for
 * the different offsets, showing the identical output.
 *
 * The outputs of the AlignLR object is then corrected, if necessary,
 * by delaying either the L or R channel.
 *
 * This test allows the analog signal to come from either the Codec
 * DAC or from a digital I/O pin on the Teensy.
 *
 */

// Beta rev 12 Mar 2022: Added normalized measures and removed threshold. Moved
// the L-R alignment to setup().

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"

//         *******    MINI CONTROL PANEL     *******
//
// Pick one, based on the analog signal source harware being used:
//#define SIGNAL_HARDWARE TP_SIGNAL_CODEC
#define SIGNAL_HARDWARE TP_SIGNAL_IO_PIN
//
// Show the Teensy pin used for both Codec and I/O pin signal source methods
#define PIN_FOR_TP 2
//
// Set threshold as needed. Examine 3 output data around update #15
// and use about half of maximum positive value.
// #define  TP_THRESHOLD 0.001f    <<<< No Longer Used
//
//                      End Control Panel

const float sample_rate_Hz = 44100.0f;
const int   audio_block_samples = 128;
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

TPinfo* pData;
uint32_t timeSquareWave = 0;   // Switch every twoPeriods microseconds
uint32_t twoPeriods;

AudioInputI2S_F32        i2sIn;
// BEWARE _ -- The SIGNAL_HARDWARE==TP_SIGNAL_CODEC is likely to be removed and
// then the constructtion of the object will be *ONLY* the simple cases
// of TwinPeak(audio_settings) or just TwinPeak
//                                 Pin or Codec       Pin,     Invert
AudioAlignLR_F32       TwinPeak(SIGNAL_HARDWARE, PIN_FOR_TP, false, audio_settings);
AudioOutputI2S_F32       i2sOut;
#ifdef PRINT_OUTPUT_DATA
AudioRecordQueue_F32     q1;
AudioRecordQueue_F32     q2;
#endif
AudioControlSGTL5000     codec;

AudioConnection_F32      connection1(i2sIn,     0, TwinPeak,  0);
AudioConnection_F32      connection2(i2sIn,     1, TwinPeak,  1);

#if SIGNAL_HARDWARE==TP_SIGNAL_CODEC
// Not realistic for a radio but useful for testing
AudioConnection_F32      connection3(TwinPeak,  2, i2sOut,    1); // DAC R
#endif

#if SIGNAL_HARDWARE==TP_SIGNAL_IO_PIN
// Not realistic for a radio but useful for testing
AudioConnection_F32      connection4(TwinPeak,  1, i2sOut,    1); // DAC R
#endif

AudioConnection_F32      connection5(TwinPeak,  0, i2sOut,    0); // DAC L

// For test, send to queue.  For real, send to receiver.
#ifdef PRINT_OUTPUT_DATA
AudioConnection_F32      connection6(TwinPeak,  0, q1, 0);
AudioConnection_F32      connection7(TwinPeak,  1, q2, 0);
#endif

void setup(void) {
   static uint32_t tMillis = millis();
   AudioMemory_F32(50, audio_settings);

   Serial.begin(100);  // Any rate
   delay(500);
   Serial.println("Twin Peaks L-R Synchronizer Test");

   codec.inputSelect(AUDIO_INPUT_LINEIN);
   codec.enable();  // This needs to preceed TwinPeak setup

#if SIGNAL_HARDWARE==TP_SIGNAL_CODEC
   Serial.println("Using SGTL5000 Codec output for cross-correlation test signal.");
#endif
#if SIGNAL_HARDWARE==TP_SIGNAL_IO_PIN
   pinMode (PIN_FOR_TP, OUTPUT);    // Digital output pin
   Serial.println("Using I/O pin for cross-correlation test signal.");
#endif

   //TwinPeak.setThreshold(TP_THRESHOLD);   Not used
   TwinPeak.stateAlignLR(TP_MEASURE);  // Comes up TP_IDLE

#if SIGNAL_HARDWARE==TP_SIGNAL_IO_PIN
   twoPeriods = (uint32_t)(0.5f + (2000000.0f / sample_rate_Hz));
   // Note that for this  hardware, the INO is 100% in charge of the PIN_FOR_TP
   pData = TwinPeak.read();       // Base data to check error
   while (pData->TPerror < 0  &&  millis()-tMillis < 2000)  // with timeout
      {
      if(micros()-timeSquareWave >= twoPeriods && pData->TPstate==TP_MEASURE)
         {
         static uint16_t squareWave = 0;
         timeSquareWave = micros();
         squareWave = squareWave^1;
         digitalWrite(PIN_FOR_TP, squareWave);
         }
      pData = TwinPeak.read();
      }
      // The update has moved from Measure to Run. Ground the PIN_FOR_TP
      //TwinPeak.stateAlignLR(TP_RUN);  // TP is done, not TP_MEASURE
      digitalWrite(PIN_FOR_TP, 0);    // Set pin to zero

      Serial.println("");
      Serial.println("Update  ------------ Outputs  ------------");
      Serial.println("Number  xNorm     -1        0         1   Shift Error State");// Column headings
      Serial.print(pData->nMeas); Serial.print(",  ");
      Serial.print(pData->xNorm, 6); Serial.print(", ");
      Serial.print(pData->xcVal[3], 6); Serial.print(", ");
      Serial.print(pData->xcVal[0], 6); Serial.print(", ");
      Serial.print(pData->xcVal[1], 6); Serial.print(", ");
      Serial.print(pData->neededShift); Serial.print(",   ");
      Serial.print(pData->TPerror); Serial.print(",    ");
      Serial.println(pData->TPstate);

  // You can see the injected signal level by the printed variable, pData->xNorm
  // It is the sum of the 4 cross-correlation numbers and if it is below 0.0001 the
  // measurement is getting noisy and un-reliable.  Raise the injected signal level
  // to solve the problem.
#endif
}

void loop(void) {
   }


