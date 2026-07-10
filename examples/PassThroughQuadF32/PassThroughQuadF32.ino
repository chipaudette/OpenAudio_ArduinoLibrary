// Demonstration of AudioOutputI2SQuad_F32 four channel I2S output object.
// Greg Raven KF5N November 2025.
// The first left and right channels are output on pin 7, which drives the
// Teensy Audio Adapter (Teensy 4.1).  The second left and right channels are output on pin 32 (Teensy 4.1).

#include <Arduino.h>
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>
#include <AudioStream_F32.h>

const int sample_rate_Hz = 48000;
const int audio_block_samples = 128;   // Always 128

AudioControlSGTL5000 sgtl5000_1;
#if 1
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);
AudioSynthWaveformSine_F32 tone0(audio_settings), tone1(audio_settings), tone2(audio_settings), tone3(audio_settings);

AudioOutputI2SQuad_F32 i2s_out(audio_settings);

AudioConnection_F32 connect0(tone0, 0, i2s_out, 0);
AudioConnection_F32 connect1(tone1, 0, i2s_out, 1);
AudioConnection_F32 connect2(tone2, 0, i2s_out, 2);
AudioConnection_F32 connect3(tone3, 0, i2s_out, 3);
#else
AudioInputI2SQuad_F32 i2s_in();
AudioOutputI2SQuad_F32 i2s_out(audio_settings);

#ifdef MIXUP
AudioConnection          patchCord1(i2s_in, 0, i2s_out, 2);
AudioConnection          patchCord2(i2s_in, 1, i2s_out, 3);
AudioConnection          patchCord3(i2s_in, 2, i2s_out, 0);
AudioConnection          patchCord4(i2s_in, 3, i2s_out, 1);
#else // loopback
AudioConnection          patchCord1(i2s_in, 0, i2s_out, 0);
AudioConnection          patchCord2(i2s_in, 1, i2s_out, 1);
AudioConnection          patchCord3(i2s_in, 2, i2s_out, 2);
AudioConnection          patchCord4(i2s_in, 3, i2s_out, 3);
	
#endif
#endif
const int myInput = AUDIO_INPUT_LINEIN;
//const int myInput = AUDIO_INPUT_MIC;

void setup() {

  Serial.begin(115200);

  /* check for CrashReport stored from previous run */
  if (CrashReport) {
    Serial.print(CrashReport);
  }

/* DMA debug code.
  uint32_t* dmaErrors;
  dmaErrors = (uint32_t*)(0x400E8000 + 0x4);
  Serial.printf("DMA errors = %u\n", *dmaErrors);
  */
#define UNMUTEAUDIO LOW
#define MUTEAUDIO   HIGH  
const int MUTE = 38;          // Mute Audio, HIGH = "On" Audio PA, LOW = Mute Audio PA off.  This may be reversed depending on PA.

  pinMode(MUTE, OUTPUT);
  digitalWrite(MUTE, MUTEAUDIO);  // Keep audio junk out of the speakers/headphones until configuration is complete.
  Serial.printf("MUTE\n");
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory_F32(20);

  // Enable the first audio shield, select input, and enable output
  sgtl5000_1.setAddress(LOW);
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(myInput);
  sgtl5000_1.volume(0.8);  // Set headphone volume.
#if 0
  // Enable the second audio shield, select input, and enable output
  sgtl5000_2.setAddress(HIGH);
  sgtl5000_2.enable();
  sgtl5000_2.inputSelect(myInput);
  sgtl5000_2.volume(0.5);
  #endif
  sgtl5000_1.unmuteHeadphone();
  Serial.printf("Please listen for tones!\n");
  tone0.amplitude(0.1);
  tone0.frequency(261.63);
  tone1.amplitude(0.1);
  tone1.frequency(329.63);
  tone2.amplitude(0.5);// 2.8Vpkpk 1.0 = 5V pkpk out
  tone2.frequency(392.0);
  tone3.amplitude(0.5);// 2.8Vpkpk 1.0 = 5V pkpk out
  tone3.frequency(440.0);
  tone0.begin();
  tone1.begin();
  tone2.begin();
  tone3.begin();
   Serial.printf("UNMUTE\n");
 
  digitalWrite(MUTE, UNMUTEAUDIO);  // Keep audio junk out of the speakers/headphones until configuration is complete.

}

void loop() {

  delay(5000);
  Serial.printf("All Off\n");
  
  // After 5 seconds, turn off all tones.
  tone0.end();
  tone1.end();
  tone2.end();
  tone3.end();

  delay(2000);
  // After 2 seconds, turn on tone0.
  Serial.printf("Tone 0 - Headphone Left - On\n");
  tone0.begin();

  delay(2000);
  // After 2 seconds, turn on tone1.
  Serial.printf("Tone 1 - Headphone Right - On\n");
  tone1.begin();

  delay(2000);
  // After 2 seconds, turn on tone2.
  Serial.printf("Tone 2- PCM5102 Left On\n");
  tone2.begin();

  delay(2000);
  // After 2 seconds, turn on tone3.
  Serial.printf("Tone 3 - PCM5102 Right - On\n");
  tone3.begin();
}
