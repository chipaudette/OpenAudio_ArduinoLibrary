/* TestEqualizer1.ino  Bob Larkin 10 May 2020
 * This is a test of the Filter Equalizer for Teensy Audio.
 * This version is for the Chip Audette _F32 Library.
 */

// Removed redundant DSP_TeensyAudio_F32.h  Bob  9 Mar 2022

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"

AudioInputI2S               i2s1;       // Start interrupts
AudioSynthWaveformSine_F32  sine1;      // Test signal
AudioFilterEqualizer_F32    equalize1;
AudioRecordQueue_F32        queue1;     // The LSB output
AudioConnection_F32         patchCord1(sine1,     0, equalize1, 0);
AudioConnection_F32         patchCord2(equalize1, 0, queue1,   0);

//nBands = 10  This octave band equalizer is set strangely to demonstrate the Equalizer
float fBand[] = {   40.0, 80.0, 160.0, 320.0, 640.0, 1280.0, 2560.0, 5120.0, 10240.0, 22058.5};
float dbBand[] = {10.0,  2.0, -2.0,  -5.0, -2.0,  -4.0,   -20.0,   6.0,    10.0,    -100};
float equalizeCoeffs[249];
float dt1[128];
float *pq1, *pd1;
int i, k;
float32_t dBResponse[500];  // Show lots of detail

void setup(void) {
  AudioMemory(5);
  AudioMemory_F32(10);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test Audio Equalizer ***");
  sine1.frequency(1000.0f);

  // Initialize the equalizer with 10 bands, 199 FIR coefficients and -65 dB sidelobes
  uint16_t eq = equalize1.equalizerNew(10, &fBand[0], &dbBand[0], 199, &equalizeCoeffs[0], 65);
  if (eq == ERR_EQ_BANDS)
    Serial.println("Equalizer failed: Invalid number of frequency bands.");
  else if (eq == ERR_EQ_SIDELOBES)
    Serial.println("Equalizer failed: Invalid sidelobe specification.");
  else if (eq == ERR_EQ_NFIR)
    Serial.println("Equalizer failed: Invalid number of FIR coefficients.");
  else
      Serial.println("Equalizer initialized successfully.");

  // Get frequency response in dB for 500 points, uniformly spaced from 0 to 21058 Hz
  equalize1.getResponse(500, dBResponse);
  Serial.println("Freq Hz, Response dB");
  for(int m=0; m<500; m++) {       // Print the response in Hz, dB, suitable for a spread sheet
    Serial.print((float32_t)m * 22058.5f / 500.0f);
    Serial.print(",");
    Serial.println(dBResponse[m]);
  }
  i = -10;  k=0;
}

void loop(void) {
  if(i<0) i++;  // Get past startup
  if(i==0) queue1.begin();

  // Collect 128 samples and output to Serial
  // This "if" will be active for i == 0
  if (queue1.available() >= 1  &&  i >= 0) {
    pq1 = queue1.readBuffer();
    pd1 = &dt1[0];
    for(k=0; k<128; k++) {
       *pd1++ = *pq1++;
     }
    i=1;   // Only collect 1 block
    Serial.print("Maximum F32 memory usage at loop:");
    Serial.println( AudioMemoryUsageMax_F32() );
    queue1.freeBuffer();
    queue1.end();  // No more data to queue1
  }
  if(i == 1) {
    // Uncomment the next 3 lines to printout a sample of the sine wave.
    /*Serial.println("128 Samples:  ");
    for (k=0; k<128; k++) {
       Serial.println (dt1[k],7);   */
  }
  i = 2;
  delay(2);
}
