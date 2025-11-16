// Demonstration of AudioOutputI2S_F32 two channel I2S output object.
// Greg Raven KF5N November 2025.
// The left and right channels are output on pin 7, which drives the
// Teensy Audio Adapter.
// A headphone can be plugged into the Audio Adapter to hear the tones.
// Be cautious plugging non-floating devices into the Audio Adapter
// headphone output.  Grounded devices may cause damage to the Audio Adapter.

#include <Arduino.h>
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>
#include <AudioStream_F32.h>

const float sample_rate_Hz = 48000.0;
const int audio_block_samples = 128;

AudioControlSGTL5000 sgtl5000_1;
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);
AudioSynthWaveformSine_F32 tone0(audio_settings), tone1(audio_settings);
AudioOutputI2S_F32 i2s_out(audio_settings);

AudioConnection_F32 connect0(tone0, 0, i2s_out, 0);
AudioConnection_F32 connect1(tone1, 0, i2s_out, 1);

void setup() {

  Serial.begin(115200);

  /* check for CrashReport stored from previous run */
  if (CrashReport) {
    Serial.print(CrashReport);
  }

  uint32_t* dmaErrors;
  dmaErrors = (uint32_t*)(0x400E8000 + 0x4);
  Serial.printf("DMA errors = %u\n", *dmaErrors);

  sgtl5000_1.enable();
  sgtl5000_1.setAddress(LOW);
  AudioMemory_F32(10);
  sgtl5000_1.volume(0.8);  // Set headphone volume.
  sgtl5000_1.unmuteHeadphone();
  Serial.printf("Please listen for tones!\n");
  tone0.amplitude(0.1);
  tone0.frequency(261.63);
  tone0.end();
  tone1.amplitude(0.1);
  tone1.frequency(329.63);
  tone1.end();

  tone0.begin();
  Serial.println("tone0 is on");
  delay(5000);
  tone0.end();
  Serial.println("tone0 is off");
  delay(5000);
  tone1.begin();
  Serial.println("tone1 is on");
  delay(5000);
  tone1.end();
  Serial.println("Both tones off");
}

void loop() {
}
