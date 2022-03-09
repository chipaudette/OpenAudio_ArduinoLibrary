/* TestTwinPeaks.ino    Bob Larkin   26 Feb 2022
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
#define  TP_THRESHOLD 11.0f
//
// Un-comment the next to print samples of the phase-adjusted L&R data
// #define PRINT_OUTPUT_DATA
//
//                      End Control Panel

const float sample_rate_Hz = 44100.0f;
const int   audio_block_samples = 128;
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

uint16_t nMeasLast = 0;
uint32_t timeSquareWave = 0;   // Switch every 45 microseconds

AudioInputI2S_F32        i2sIn;
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
AudioConnection_F32      connection4(TwinPeak,  2, i2sOut,    0); // DAC L
AudioConnection_F32      connection6(TwinPeak,  2, i2sOut,    1); // DAC R
#endif

// For test, send to queue.  For real, send to receiver.
#ifdef PRINT_OUTPUT_DATA
AudioConnection_F32      connectionA(TwinPeak,  0, q1, 0);
AudioConnection_F32      connectionB(TwinPeak,  1, q2, 0);
#endif

void setup(void) {
   AudioMemory_F32(30, audio_settings);
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

   TwinPeak.setThreshold(TP_THRESHOLD);
   TwinPeak.stateAlignLR(TP_MEASURE);  // Comes up TP_IDLE

   Serial.println("");
   Serial.println("Update ------- Outputs  -------");
   Serial.println("Number  -1        0         1   Shift Error");// Column headings
}

void loop(void) {
   // The following, under PRINT_OUTPUT_DATA, is an output of the L & R channels
   // suitable for examination in a spreadsheet.
#ifdef PRINT_OUTPUT_DATA
static int32_t ii=0;
   if(ii==5)
      {
      q1.begin();
      q2.begin();
      }
   if(ii>5 && ii<15)
      {
	   if(q1.available())
	      {
		   Serial.println(" ====================");
	      float* pf1 = q1.readBuffer();
	      for(int mm=0; mm<128; mm++)
		      Serial.println(*(pf1 + mm),5);
		   q1.freeBuffer();
		   }
		Serial.println("^--L");
		if(q2.available())
	      {
	      float* pf2 = q2.readBuffer();
	      for(int mm=0; mm<128; mm++)
		      Serial.println(*(pf2 + mm),5);
		   q2.freeBuffer();
		   }
		Serial.println("^--R");
	    }
   if(ii==16)
      {
      q1.end();
      q2.end();
   ii++;
      }
#endif

   // uint32_t tt=micros();
   TPinfo* pData = TwinPeak.read();
   if(pData->nMeas > nMeasLast && pData->nMeas<20)
      {
      nMeasLast = pData->nMeas;     // This print takes about 6 microseconds
      Serial.print(pData->nMeas); Serial.print(", ");
      Serial.print(pData->xcVal[3], 6); Serial.print(", ");
      Serial.print(pData->xcVal[0], 6); Serial.print(", ");
      Serial.print(pData->xcVal[1], 6); Serial.print(", ");
      Serial.print(pData->neededShift); Serial.print(",   ");
      Serial.println(pData->TPerror);
      //Serial.println(pData->TPstate);
#if SIGNAL_HARDWARE==TP_SIGNAL_IO_PIN
      if(pData->TPerror == 0)
         {
         TwinPeak.stateAlignLR(TP_RUN);  // TP is done
         digitalWrite(PIN_FOR_TP, 0);
         }
#endif
      // Serial.println(micros()-tt);
      }

#if SIGNAL_HARDWARE==TP_SIGNAL_IO_PIN
   // Generate 11.11 kHz square wave
   // For other sample rates, set to roughly 2 sample periods, in microseconds
   if(micros()-timeSquareWave >= 45 && pData->TPstate==TP_MEASURE)
      {
      static uint16_t squareWave = 0;
      timeSquareWave = micros();
      squareWave = squareWave^1;
      digitalWrite(PIN_FOR_TP, squareWave);
      }
#endif

   }
