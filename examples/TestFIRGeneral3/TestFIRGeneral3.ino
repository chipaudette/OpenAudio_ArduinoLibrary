/* TestEqualizer3General.ino  Bob Larkin 22 May 2020
 * This is a test of the Filter Equalizer for Teensy Audio.
 * This version is for the Chip Audette _F32 Library.
 * See TestEqualizer2.ino for the int16 version.
 */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "DSP_TeensyAudio_F32.h"

AudioInputI2S               i2s1;
AudioSynthWaveformSine_F32  sine1;      // Test signal
AudioFilterFIRGeneral_F32   firg1;
AudioRecordQueue_F32        queue1;     // The LSB output
AudioConnection_F32         patchCord1(sine1,     0, firg1, 0);
AudioConnection_F32         patchCord2(firg1, 0, queue1,    0);

float dbA[51];
float equalizeCoeffs[51];
float dt1[128];
float *pq1, *pd1;
int i, k;
float32_t dBResponse[500];  // Show lots of detail

void setup(void) {
  AudioMemory(5); 
  AudioMemory_F32(10);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test FIR General routine for F32 ***");
  sine1.frequency(1000.0f);

  // This simple case corresponds to Burris and Parks, "Digital Filter Design."
  // Example 3.1when sidelobes are set to 0.0.
  for (i=0;  i<6; i++) dbA[i] =   0.0f;
  for (i=6; i<11; i++) dbA[i] = -120.0f;
  
  // Initialize the FIR with 21 points, 21 FIR coefficients and -0 dB sidelobes
  uint16_t eq = firg1.FIRGeneralNew(&dbA[0], 21, &equalizeCoeffs[0], 0.0);
  if (eq == ERR_EQ_BANDS)
    Serial.println("FIR General failed: Invalid number of frequency points.");
  else if (eq == ERR_EQ_SIDELOBES)
    Serial.println("FIR General failed: Invalid sidelobe specification.");
  else if (eq == ERR_EQ_NFIR)
    Serial.println("FIR General failed: Invalid number of FIR coefficients.");
  else
    Serial.println("FIR General initialized successfully.");
    
  Serial.println("FIR Coeffs");
  for(i=0; i<21; i++) Serial.println(equalizeCoeffs[i], 7);
  Serial.println("");

  // Get frequency response in dB for 200 points, uniformly spaced from 0 to 21058 Hz
  firg1.getResponse(200, dBResponse);
  Serial.println("Freq Hz, Response dB");
  for(int m=0; m<200; m++) {       // Print the response in Hz, dB, suitable for a spread sheet
    Serial.print((float32_t)m * 22058.5f / 200.0f);
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
    queue1.freeBuffer();
    queue1.end();  // No more data to queue1
  }
  if(i == 1) { 
    // Uncomment the next 3 lines to printout a sample of the sine wave.
    Serial.println("128 Samples:  ");
    for (k=0; k<128; k++)
      Serial.println (dt1[k],7); 
    i = 2;
  }
  delay(2);
}
