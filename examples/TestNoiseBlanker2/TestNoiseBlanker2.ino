/* TestNoiseBlanker.ino  Bob Larkin 20 May 2020
 * 
 * Sine wave plus noise spike test of radioNoiseBlanker_F32
 * Feeds input to both 0 and 1 channels (I & Q or L & R).
 * The 0 channel controls the noise blanking, but both paths are
 * noise-blanked.  The function useTwoChannels(true) enables the 
 * second path. The default case is to use only the 0 path for everything.
 */

#include "AudioStream_F32.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "Arduino.h"
#include "Audio.h"

#include "radioNoiseBlanker_F32.h"

AudioInputI2S_F32            i2sIn1;
//AudioSynthGaussian_F32       gwn1;
AudioPlayQueue_F32           playq1;
radioNoiseBlanker_F32        nb1;
AudioRecordQueue_F32         queue1;

AudioConnection_F32    pcord0(playq1,  0, nb1,     0);
AudioConnection_F32    pcord1(playq1,  0, nb1,     1);

//  The next two should be identical data outputs.  Pick ONLY ONE at a time
//AudioConnection_F32    pcord2(nb1,     0, queue1,  0);
AudioConnection_F32    pcord2(nb1,     1, queue1,  0);

float32_t *pin;
float32_t dt1[128];
float32_t *pq1, *pd1;
int i;

// Fake noise pulses
float32_t pulse[] = 
{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 
 0.9f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
 
void setup(void) {
  AudioMemory_F32(25);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test Noise Blanker ***");
  
  // setNoiseBlanker(float threshold, uint16_t nAnticipation, uint16_t nDecay)
  nb1.setNoiseBlanker(4.0f, 2, 3);

  //nb1.showError(1);
  nb1.useTwoChannel(true);  // true enables a path trrough the "1" side
  nb1.enable(true);

  while(pin == NULL)
      pin = playq1.getBuffer();
  Serial.println("Input to NB:");
  for(int k=0; k<128; k++)  {  // Signal and noise
      pin[k] = pulse[k] + 0.1*sinf(0.6*(float32_t)k);
      Serial.println(pin[k], 6);
  }
  playq1.playBuffer();  // Put 128 data into stream
  queue1.begin();
  i = 0;
}

void loop(void) {
  // Collect 128 samples and output to Serial
  if (queue1.available() >= 1) {  // See if it has arrived
    pq1 = queue1.readBuffer();
    pd1 = &dt1[0];
    for(int k=0; k<128; k++) {
       *pd1++ = *pq1++;
     }
    i=1;   // data into dt1[]
    queue1.freeBuffer();
    queue1.end();  // No more data to queue1
  }
  if(i == 1) { 
     // Printout 128 samples of the gated signal.
     Serial.println("128 NB Output Samples:  ");
     for (int k=0; k<128; k++) {
        Serial.println (dt1[k], 9);
     }
     i = 2;
  }
}
