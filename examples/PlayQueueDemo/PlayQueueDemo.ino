/*          I16 EXAMPLE CONVERTED TO F32
 * A simple queue test which generates a sine wave in the
 * application and sends it to headphone jack. The results
 * should always be a glitch-free wave, but various options
 * can be changed to modify the usage of audio blocks, and an
 * ability to execute the loop() function more slowly, with
 * simple waveform generation, or more efficiently / faster,
 * but requring some programmer effort to re-try if sending
 * waveform data fails due to a lack of buffer or queue space.
 *
 * This example code is in the public domain.
 *
 * Jonathan Oakley, November 2021
 * Converted from I16 to F32 - Bob Larkin Feb 2023.  Thanks, Jonathan
 * 18 Mar 2023: Includes corrections to testMode==1, NON-STALLING from
 * https://github.com/h4yn0nnym0u5e/Audio/commit/9fce95edb634891ce6d28288e87f54f4994096e8
 */

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include "Arduino.h"
#include "play_queue_f32.h"

AudioPlayQueue_F32         queue1;         //xy=917,329
AudioOutputI2S_F32         i2s2;           //xy=1121,330
AudioConnection_F32        patchCord1(queue1, 0, i2s2, 0);
AudioConnection_F32        patchCord2(queue1, 0, i2s2, 1);
AudioControlSGTL5000       sgtl5000_1;     //xy=1131,379

#define COUNT_OF(a) (sizeof a / sizeof a[0])
uint32_t next;

// ************  SETUP()  **************
void setup() {
  pinMode(13,OUTPUT);

  Serial.begin(115200);
  delay(1000);
  Serial.println("AudioPlayQueue_F32 Test");

  if (CrashReport)
  {
    Serial.println(CrashReport);
    CrashReport.clear();
  }

  AudioMemory_F32(100);

  sgtl5000_1.enable();
  sgtl5000_1.volume(0.5);

  next = millis();

  // Comment the following out (or set to ORIGINAL) for old stall behaviour;
  // set to NON_STALLING for return with status if audio blocks not available,
  // or no room in queue for another audio block.
  queue1.setBehaviour(AudioPlayQueue_F32::NON_STALLING);  // <=== Set
  // queue1.setBehaviour(AudioPlayQueue_F32::ORIGINAL);      // or this
  queue1.setMaxBuffers(4);
}

/*
 * Generate one sample of a waveform.
 * Currently 220Hz sine wave, but could make it more complex.
 */
uint32_t genLen;

float32_t nextSample()
{
  static float phas = 0.0f;
  float32_t amp = 0.05f;
  float32_t result = amp*sinf(phas);
  genLen++;

  phas += 220./AUDIO_SAMPLE_RATE_EXACT*TWO_PI;
  if (phas > TWO_PI)
    phas -= TWO_PI;
  return result;
}

int loops;
int nulls,nulls2;
int testMode = 2; // 1: getBuffer / playBuffer; 2: play(), mix of samples and buffers  <<<SET
int playMode; // 1: generate individual samples and send; 2: generate buffer of samples and send
float32_t samples[512],*sptr; // space for samples when using play()
uint32_t len; // number of buffered samples (remaining)
int noQueueSpace; // did call to AudioPlayQueue::playBuffer() fail? If so, re-try
int noQcount; // count of times we had to wait to queue a block

void loop() {

  switch (testMode)
  {
    case 1: // use getBuffer / playBuffer
    {
      if (0 != noQueueSpace)
         noQueueSpace = queue1.playBuffer();
      else
      {
        float32_t* buf = queue1.getBuffer();

        if (NULL == buf)
          nulls++;
        else
        {
          for (int i=0; i<AUDIO_BLOCK_SAMPLES; i++)
            buf[i] = nextSample();
          noQueueSpace = queue1.playBuffer();
        }
      }
      if (noQueueSpace)
        noQcount++;
    }
      break;

    case 2: // use play()
    {
      if (0 == len)
      {
        playMode = random(2)+1; // 1 or 2
      }

      switch (playMode)
      {
        case 1: // send single samples
          if (0 == len)
          {
            len = random(10,2*AUDIO_BLOCK_SAMPLES+1); // be deliberately awkward
            samples[0] = nextSample();
          }

          while (len > 0)
          {
            if (0 == queue1.play(samples[0])) // sent a sample...
            {
              len--;        // count down
              if (len > 0)  // if more to send...
                samples[0] = nextSample(); // ...create the next one
            }
            else
            {
              nulls++; // count up the re-tries
              break;
            }
          }
          break;

         case 2: // send a block of samples
          if (0 == len)
          {
            len = random(10,COUNT_OF(samples)); // be deliberately awkward
            for (unsigned int i=0;i<len;i++) // pre-fill the buffer
              samples[i] = nextSample();
            sptr = samples;
          }

          {
            uint32_t left = queue1.play(sptr,len); // send samples, maybe
            if (left > 0) // not everything went
            {
              sptr += len - left; // advance the pointer
              nulls2++; // count up the re-tries
            }
            len = left;
          }
          break;
      }
    }
      break;
  }

  loops++;
  if (millis() > next)
  {
    next += 100; // aim to output every 100ms

    // In NON_STALLING mode this loops really fast, and the millis() value goes up by
    // 100 on every output line. In ORIGINAL mode the loop is slow, and the internal
    // stall results in slightly unpredictable timestamps.
    Serial.printf("%d: millis = %d, loops = %d, nulls = %u, nulls2 = %u, samples = %u, wait count = %d\n",
                  playMode, millis(),loops, nulls, nulls2, genLen, noQcount);
  }
}
