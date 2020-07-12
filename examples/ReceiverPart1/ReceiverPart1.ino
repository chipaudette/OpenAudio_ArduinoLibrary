/* ReceiverPart1.ino  Bob Larkin 20 April 2020
 * This is "Hello World" of Radio design.  It can
 * receive a Lower Sideband signal, but that is it.  
 * To test the floating point radio receiver blocks:
 *   AudioFilter90Deg_F32
 *   RadioIQMixer_F32
 */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"

AudioInputI2S               i2s1;
AudioSynthWaveformSine_F32  sine1;      // Test signal
RadioIQMixer_F32            iqmixer1;
AudioFilter90Deg_F32        hilbert1;
AudioMixer4_F32             sum1;
AudioRecordQueue_F32        queue1;     // The LSB output
AudioRecordQueue_F32        queue2;     // The test sine wave

AudioConnection_F32         patchCord0(sine1,    0, iqmixer1, 0);
AudioConnection_F32         patchCord2(sine1,    0, iqmixer1, 1);
AudioConnection_F32         patchCord4(iqmixer1, 0, hilbert1, 0);
AudioConnection_F32         patchCord5(iqmixer1, 1, hilbert1, 1);
AudioConnection_F32         patchCord6(hilbert1, 0, sum1,     0);
AudioConnection_F32         patchCord7(hilbert1, 1, sum1,     1);
AudioConnection_F32         patchCord8(sum1,     0, queue1,   0);
AudioConnection_F32         patchCord9(sine1,    0, queue2,   0);

#include "hilbert19A.h"
#include "hilbert121A.h"
#include "hilbert251A.h"

float dt1[128];
float dt2[128];
float *pq1, *pd1, *pq2, *pd2;
int i, k;

void setup(void) {
  AudioMemory(5); 
  AudioMemory_F32(10);
  Serial.begin(300);  delay(1000);

  sine1.frequency(14000.0);
  iqmixer1.frequency(16000.0);
  // Pick one of the three. Note that the 251 does not have
  // enough samples here to show the full build-up.
  // hilbert1.begin(hilbert19A, 19);
  hilbert1.begin(hilbert121A, 121);
  // hilbert1.begin(hilbert251A, 251);
  sum1.gain(0, 1.0);   // Leave at 1.0
  sum1.gain(1, -1.0);  // -1 for LSB out
  iqmixer1.showError(1);   // Prints update() errors
  hilbert1.showError(1);
  queue1.begin();
  queue2.begin();
  i = 0;  k=0;
}

void loop(void) {
  // Collect 128 samples and output to Serial
  // This "if" will be active for i == 0
  if (queue1.available() >= 1  && i==0) {
    pq1 = queue1.readBuffer();
    pd1 = &dt1[0];
    pq2 = queue2.readBuffer();
    pd2 = &dt2[0];
    for(k=0; k<128; k++) {
      *pd1++ = *pq1++;
      *pd2++ = *pq2++; 
    }
    i=1;   // Only collect 1 block
    Serial.print("Maximum F32 memory usage at loop:");
    Serial.println( AudioMemoryUsageMax_F32() );
    queue1.freeBuffer();
    queue2.freeBuffer();
    queue1.end();  // No more data to queue1
    queue2.end();  // No more data to queue2
  }
  if(i == 1) { 
    Serial.println("128 Samples: Time (sec), LSB Out, Sine Wave In");
    for (k=0; k<128; k++) {
      Serial.print (0.000022667*(float32_t)k, 6);   Serial.print (",");
      Serial.print (dt1[k],7);  Serial.print (",");
      Serial.println (dt2[k],7);
    }
    i = 2;
  }
}
