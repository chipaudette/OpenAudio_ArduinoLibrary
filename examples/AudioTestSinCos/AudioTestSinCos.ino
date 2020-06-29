/* AudioTestSinCos.ino  Bob Larkin 19 April 2020
 * Generates 1024 samples of Sin and Cos from the
 * AudioSynthSinCos_F32.  Samples sent to the USB 
 * Serial.print.  Also, the sine and cosine signals are
 * connected to the AudioAnalyzePhase_32 and a third
 * Queue to Serial.print the, close to 90 degree,
 * phase difference.   
 */

#include "Audio.h"
#include <OpenAudio_ArduinoLibrary.h>
#include "DSP_TeensyAudio_F32.h"

#define NBLOCKS 8
// Necessary foraudio stream
AudioInputI2S               i2s1;
AudioConvert_I16toF32       convert; 
AudioConnection             pcI16(i2s1, 0, convert, 0);
// And the experiment
AudioSynthSineCosine_F32    sincos1;
AudioRecordQueue_F32        queue1;
AudioRecordQueue_F32        queue2;
AudioRecordQueue_F32        queue3;
AudioAnalyzePhase_F32       phase1;
AudioConnection_F32         patchCord1(sincos1, 0, queue1, 0);
AudioConnection_F32         patchCord2(sincos1, 1, queue2, 0);
AudioConnection_F32         patchCord3(sincos1, 0, phase1, 0);
AudioConnection_F32         patchCord4(sincos1, 1, phase1, 1);
AudioConnection_F32         patchCord5(phase1,  0, queue3, 0);

float dt1[128*NBLOCKS];    // Place to save sin
float dt2[128*NBLOCKS];    // and cos
float dt3[128*NBLOCKS];    // and phase angle
float *pq1, *pd1, *pq2, *pd2, *pd3, *pq3;
int i, k;

void setup(void) {
  AudioMemory(5);       //allocate Int16 audio data blocks
  AudioMemory_F32(20);  //allocate Float32 audio data blocks
  Serial.begin(300);  delay(1000);

  // simple() is not needed here, as it is default unless amplitude or
  // phaseS_C_r is changed.  But, it can be used to restore simple after
  // involking changes.  Here it is just a sample of usage.
  sincos1.simple(true);
  // Default amlitude +/- 1.0
  sincos1.frequency(1212.345);
  // If either or both of the following two are uncommented, the
  // detailed, slower, sincos will be used:
  // sincos1.amplitude(0.9);
  // sincos1.phaseS_C_r(120.00*M_PI/180.0); // Test non-90 degree, like 120 deg

  // Set next to 1 to  print errors in update(), or 0 for no print.  For debug only
  phase1.showError(0);
  
  queue1.begin();
  queue2.begin();
  queue3.begin();
  i = 0;  k=0;
}

void loop(void) {
  // Collect 128xNBLOCKS samples and output to Serial
  // This "if" will be active for i on (0, NBLOCKS-1)
  if (queue1.available() >= 1 && queue2.available()
          && queue3.available() && i>=0 && i<NBLOCKS) {
    pq1 = queue1.readBuffer();
    pd1 = &dt1[128*i];
    pq2 = queue2.readBuffer();
    pd2 = &dt2[128*i];
    pq3 = queue3.readBuffer();
    pd3 = &dt3[128*i++];
    for(k=0; k<128; k++) {
      *pd1++ = *pq1++;        // Save 128 words in dt1[]
      *pd2++ = *pq2++; 
      *pd3++ = *pq3++; 
    }
    queue1.freeBuffer();
    queue2.freeBuffer();
    queue3.freeBuffer();
  }
  if(i == NBLOCKS) {   // Wait for all NBLOCKS 
    i = NBLOCKS + 1;   // Should stop data collection
    queue1.end();  // No more data to queue1
    queue2.end();  // No more data to queue2

    Serial.println("1024 Sine, Cosine, Phase Data Points:");
    for (k=0; k<128*NBLOCKS; k++) {
      Serial.print (dt1[k],7);
      Serial.print (",");
      Serial.print (dt2[k],7);
      Serial.print (",");
      Serial.println(dt3[k],7);
    }
  }
}
