/* ReceiverFM.ino    Bob Larkin   26 April 2020
 * This is a simple test of introducing a sine wave to the
 * FM Detector and taking 512 samples of the output.  It is
 * a static test with a fixed frequency for test and so
 * the output DC value and noise can be tested.  Note that the 512
 * samples include the startup transient, so the first 300, 
 * or so, points should be ignored in seeing the DC value.
 * 
 * Change the value of sine1.frequency to see the DC output change.
 * See FMReceiver2.ino for testing with real AC modulation.
 * 
 * As an alternative the input can come from the ADC for "SINE_ADC 0"
 * 
 * Output is sent to left channel SGTL5000 DAC.
 */

#include "Audio.h"
#include <OpenAudio_ArduinoLibrary.h>

// SINE_ADC 1 for internally generated sine wave.
// SINE_ADC 0 to use the SGTL5000 Teensy audio adaptor ADC/DAC
#define SINE_ADC 0

#if SINE_ADC
AudioSynthWaveformSine_F32  sine1;
RadioFMDetector_F32         fmDet1;
AudioRecordQueue_F32        queue1;
AudioConvert_F32toI16       cnvrtOut;
AudioOutputI2S              i2sOut;
AudioControlSGTL5000        sgtl5000_1;
AudioConnection_F32    connect1(sine1,    0, fmDet1,   0);
AudioConnection_F32    connect3(fmDet1,   0, cnvrtOut, 0);
AudioConnection        connect4(cnvrtOut, 0, i2sOut,   0); // left
AudioConnection_F32    connect5(fmDet1,   0, queue1,   0);
#else            // Input from Teensy Audio Adaptor SGTL5000
// Note - With no input, the FM detector output is all noise.  This
// can be loud, so one can add a gain block at the fmDet1 output (like 0.05 gain).
AudioInputI2S          i2sIn;
AudioConvert_I16toF32  cnvrtIn;
RadioFMDetector_F32    fmDet1;
AudioRecordQueue_F32   queue1; 
AudioConvert_F32toI16  cnvrtOut;
AudioOutputI2S         i2sOut;
AudioControlSGTL5000   sgtl5000_1;
AudioConnection        connect1(i2sIn,    0, cnvrtIn,  0); // left
AudioConnection_F32    connect2(cnvrtIn,  0, fmDet1,   0);
AudioConnection_F32    connect3(fmDet1,   0, cnvrtOut, 0);
AudioConnection_F32    connect5(fmDet1,   0, queue1,   0);
AudioConnection        connect7(cnvrtOut, 0, i2sOut,   0);
#endif

float dt1[512];    // Place to save output
float *pq1, *pd1;
uint16_t k;
int i;

void setup(void) {
  AudioMemory(5);
  AudioMemory_F32(5);
  Serial.begin(300);  delay(1000);  // Any rate is OK
  Serial.println("Serial Started");
 
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);   

#if SINE_ADC
  sine1.frequency(14000.0);
#endif

  // The FM detector has error checking during object construction
  // when Serial.print is not available.  See RadioFMDetector_F32.h:
  Serial.print("FM Initialization errors: ");
  Serial.println( fmDet1.returnInitializeFMError() );

  // The following enables error checking inside of the "ubdate()"
  // Output goes to the Serial (USB) Monitor.  Normally, this is quiet.
  fmDet1.showError(1);
  
  queue1.begin(); 
  i = 0;  k=0;
}

void loop(void) {
  // Collect 512 samples and output to Serial
  // This "if" will be active for i = 0,1,2,3
  if (queue1.available() >= 1) {
    if( i>=0 && i<4) {
      pq1 = queue1.readBuffer();
      pd1 = &dt1[i*128];
      for(k=0; k<128; k++) {
        *pd1++ = *pq1++; 
      }
      queue1.freeBuffer();
      if(i++==3) {
         i=4;   // Only collect 4 blocks
         queue1.end();  // No more data to queue1
      }
    }
    else {
      queue1.freeBuffer();
    }
  }
  // We have 512 data samples.  Serial.print them
  if(i == 4) {
#if SINE_ADC
	Serial.println("For 14,000 Hz sine wave input:");
#endif
	Serial.println("512 samples of FM Det output, starting t=0");
    Serial.println("Time in sec, FM Output, Dev from 15,000 Hz:");
    for (k=0; k<512; k++) {
       Serial.print (0.000022667*(float32_t)k, 6);   Serial.print (", ");
       Serial.print (dt1[k],7);   Serial.print (", ");
       Serial.println (dt1[k]/0.000142421, 2);  // Convert to Hz
    }
    i = 5;
  }
  if(i==5) {
    i = 6;
    Serial.print("CPU: Percent Usage, Max: ");
    Serial.print(AudioProcessorUsage());
    Serial.print(", ");
    Serial.println(AudioProcessorUsageMax());

    Serial.print("Int16 Memory Usage, Max: ");
    Serial.print(AudioMemoryUsage());
    Serial.print(", ");
    Serial.println(AudioMemoryUsageMax());

    Serial.print("Float Memory Usage, Max: ");
    Serial.print(AudioMemoryUsage_F32());
    Serial.print(", ");
    Serial.println(AudioMemoryUsageMax_F32());
    Serial.println();
  }
}
