/*
 *  ToneDetect1.ino  Test the undecimated tone detector.
 *
 * Note that there is an alternative AudioAnalyzeCTCSS_F32
 * tone detector for frequencies below a few hundred Hz.  This
 * uses /16 decimation.
 *
 * This detector is suitable for DTMF  or similar tones.
 *
 * Bob Larkin 26 March 2021
 * Rev 10 Mar 2022 to be compatible with CTCSS detector.
 *
 * Public Domain
 */
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will handle.
const float sample_rate_Hz = 44117.0f ;  // 24000, 44117, or other frequencies listed above (untested)
const int   audio_block_samples = 128;   // Others untested
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples); // Not used, all at default

AudioSynthWaveformSine_F32  sine1;
AudioAnalyzeToneDetect_F32  toneDet1;
AudioOutputI2S_F32       audioOutI2S1;
AudioConnection_F32      patchCord3(sine1, toneDet1);

float freq;

void setup() {
  Serial.begin(300);
  delay(1000);
  Serial.println("OpenAudio_ArduinoLibrary  - TestTone Detector");

  AudioMemory_F32(20, audio_settings);

  toneDet1.frequency(1209.0f, 24);
  sine1.amplitude(1.0f);

  // Plot the data from this loop to see the "sin(f)/f" response
  for(freq=1000.0f; freq<=1400.0f; freq += 2.0f)
      measureDataPoint();
}

void loop() {
 }

void measureDataPoint(void) {
   sine1.frequency(freq);
   // The toneDet1 is continuous.  But, if we change frequency, we need to
   // wait at least 20 cycles at the tone detector frequency
   // for the old measurements to flush through.  At 200 Hz toneDet1
   // frequency, that would be 100 mSec.  Thus, 120 msec should be enough.
   delay(120);
   if( toneDet1.available() )  {
      Serial.print(freq, 2);
      Serial.print(",");
      Serial.println(20.0f*log10f(toneDet1.read()), 6);
      }
  }
