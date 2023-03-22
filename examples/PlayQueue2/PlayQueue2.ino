/*     PlayQueue2.ino   Simple Example of Non-Stalling use.
 *
 * A simple queue test which generates a sine wave in the
 * application and sends it to the Codec. The results
 * should always be a glitch-free wave.
 *
 * This example is the most basic PlayQueue example, and only uses the
 * NON-STALLING behavior.  It has been cut down from Jonathan Oakley's
 * I16 example PlayQueueDemo.ino
 * https://github.com/h4yn0nnym0u5e/Audio/blob/bugfix/PlayQueueDemo/examples/Queues/PlayQueueDemo/PlayQueueDemo.ino
 *
 * This example code is in the public domain.
 * Bob Larkin March 2023
 */

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include "Arduino.h"
//#include "play_queue_f32.h"

AudioPlayQueue_F32         queue1;         //xy=917,329
AudioOutputI2S_F32         i2s2;           //xy=1121,330
AudioConnection_F32        patchCord1(queue1, 0, i2s2, 0);
AudioConnection_F32        patchCord2(queue1, 0, i2s2, 1);
AudioControlSGTL5000       sgtl5000_1;     //xy=1131,379

uint32_t next;
int loops;
int nulls2;
// For this example, we can always fill samples[], as needed.
// In other cases, there might be delays in getting data. Then
// the size of this buffer could be made larger to compensate.
float32_t samples[128],*sptr; // space for samples when using play()
uint32_t len; // number of buffered samples remaining

uint32_t genLen;

void setup() {
   Serial.begin(115200);
   delay(1000);
   Serial.println("PlayQueue2 F32 Test");

   if (CrashReport)
      {
      Serial.println(CrashReport);
      CrashReport.clear();
      }
   AudioMemory_F32(10);

   sgtl5000_1.enable();
   sgtl5000_1.volume(0.5);

   next = millis();
   // set to NON_STALLING for return with status if audio blocks not available,
   // or no room in queue for another audio block.
   queue1.setBehaviour(AudioPlayQueue_F32::NON_STALLING);
   queue1.setMaxBuffers(4);
   }

// Generate one sample of a 220 Hz sine wave waveform.
float32_t nextSample()  {
   static float phas = 0.0f;
   float32_t amp = 0.05f;
   float32_t result = amp*sinf(phas);
   genLen++;

   phas += 220./AUDIO_SAMPLE_RATE_EXACT*TWO_PI;
   if (phas > TWO_PI)
      phas -= TWO_PI;
   return result;
   }

void loop() {
   if (0 == len)
      {
      len = 128;
      for (uint16_t i=0; i<len; i++) // pre-fill the buffer
         samples[i] = nextSample();
      sptr = samples;
      }

	// For NON_STALLING, used here, play() will return a non-zero value
   // if no queue space is free. This is the number of floats that were
   // not sent.  For this case the call must be re-tried again, waiting
   // forplay to have occurred.
   uint32_t left = queue1.play(sptr,len); // send samples, maybe
   if (left > 0) // not everything went
      {
      sptr += len - left; // advance the pointer
      nulls2++; // count up the re-tries
      }
   len = left;
   loops++;
   if (millis() > next)
      {
      next += 500; // Output every 500ms
      // In NON_STALLING mode this loops really fast
      Serial.printf("millis = %d,  loops = %d,  nulls2 = %u,  samples = %u\n",
                    millis(),loops, nulls2, genLen);
      }
   }
