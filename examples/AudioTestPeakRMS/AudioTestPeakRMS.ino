/* AudioTestPeakRMS.ino  Bob Larkin 2 May 2020
 *  
 * Generates sine wave and measures RMS, Peak and Peak
 * to Peak values.
 * NOTE:  This is for the floating point _F32 versions of
 * AnalyzePeak and AnalyzeRMS *NOT* the Teensy Audio Library
 * versions with 16-bit Fixed point data blocks.
 */

#include "Audio.h"
#include <OpenAudio_ArduinoLibrary.h>
#include "analyze_peak_f32.h"
#include "analyze_rms_f32.h"

AudioInputI2S               i2s1; 
AudioSynthWaveformSine_F32  sine1;
AudioAnalyzeRMS_F32         rms1;
AudioAnalyzePeak_F32        peak1;
AudioConnection_F32         patchCord1(sine1, 0, rms1, 0);
AudioConnection_F32         patchCord2(sine1, 0, peak1, 0);

uint16_t n = 0;

void setup(void) {
  AudioMemory(5);       //allocate Int16 audio data blocks
  AudioMemory_F32(5);   //allocate Float32 audio data blocks
  Serial.begin(300);  delay(1000);

  // Default amlitude +/- 1.0
  sine1.frequency(1000.0);

  // Set next to 0 to suppress print errors in update()
  rms1.showError(1);
  peak1.showError(1);
}

void loop(void) {
  if (n & 1) {
    while(!rms1.available() ) ;   //Wait
    Serial.print("RMS value = ");
    Serial.println(rms1.read(), 7 );
    while(!peak1.available() ) ;   //Wait
    Serial.print("Peak value = ");
    Serial.println(peak1.read(), 7 );
  }
  else  {
    while(!rms1.available() ) ;   //Wait
    Serial.print("RMS value = ");
    Serial.println(rms1.read(), 7 );
    while(!peak1.available() ) ;   //Wait
    Serial.print("Peak to peak value = ");
    Serial.println(peak1.readPeakToPeak(), 7 );
  }
  n++;
  // The RMS and Peak data collection runs during
  // delay() because of hardware interrupts
  delay(1000); 
}
