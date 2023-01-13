// WavFilePlayer2.ino
//
// WAV file player, stereo, 44.1 ksps example
// Output is Digital I2S - Used with the audio shield:
//
// The SD card may connect to different pins, depending on the
// hardware you are using.  Configure the SD card
// pins to match your hardware.  It is set for T4.x Rev D PJRC
// Teensy Audio Adaptor card here.
//
// Your microSD card must have the WAV files loaded to it:
//    SDTEST1.WAV
//    SDTEST2.WAV
//    SDTEST3.WAV
//    SDTEST4.WAV
// These are PJRC Teensy I16 library test files and are at
//   http://www.pjrc.com/teensy/td_libs_AudioDataFiles.html
//
// INO based on PJRC file of the same name, but converted to work with the
// OpenAudio_ArduinoLibrary with floating point F32 audio.
// Provision has been added for
// WAV files with sub-multiple sampling rates, relative to Audio rate.
// The latter adds an optional .INO defined FIR filter just before the
// audio is transmitted from the AudioSDPlayer_F32 update function.  This
// filter is needed for sub-multiple WAV rates, but is available for any
// place it is helpful.  Comments to Bob Larkin.  Jan 2023
//
// This example is stereo only, but demonstrates 12 ksps sampling
// in the WAV file.  See the other example, wavPlayer.ino
// that uses monaural files with 2 different sample rates.
//
// This example code is in the public domain.

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "OpenAudio_ArduinoLibrary.h"

//  This INO is setup to run audio sample rate at 44.1 ksps

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will hanedle.
const float sample_rate_Hz = 44100.0f;
const int   audio_block_samples = 128;  // Always 128, which is AUDIO_BLOCK_SAMPLES from AudioStream.h
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

AudioSDPlayer_F32            playWav1(audio_settings);
AudioAnalyzeRMS_F32          rms1;
AudioAnalyzeRMS_F32          rms2;
AudioOutputI2S_F32           audioOutput(audio_settings);
AudioConnection_F32          patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection_F32          patchCord3(playWav1, 0, rms1, 0);
AudioConnection_F32          patchCord4(playWav1, 1, rms2, 0);
AudioConnection_F32          patchCord2(playWav1, 1, audioOutput, 1);
AudioControlSGTL5000         sgtl5000_1;

// Use these with the Teensy 4.x Rev D Audio Shield (NOT for T3.x)
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  11
#define SDCARD_SCK_PIN   13

// subMult allows slower rates for the WAV files.  Not used here
//struct subMult wavNormal = {1, 0, NULL, NULL, NULL};

// wavData is a global struct, definined in AudioSDPlayer_F32.h
// This provides information about the current WAV file to this .INO
struct wavData* pCurrentWavData;

void setup() { //        **********  SETUP  **********
  Serial.begin(9600); delay(1000);
  Serial.println("*** F32 WAV from SD Card  ***");

  AudioMemory_F32(30, audio_settings);

  pCurrentWavData = playWav1.getCurrentWavData();

  sgtl5000_1.enable();
  audioOutput.setGain(0.05);    // <<< Output volume control

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  Serial.print("SD.begin() returns ");
  Serial.println(SD.begin(SDCARD_CS_PIN));  // Opens the microSD card
}

void playFile(const char *filename) {
  Serial.println("");
  Serial.print("Playing file: ");
  Serial.println(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playWav1.play(filename);

  // A brief delay for the library read WAV info
  delay(25);

  Serial.print("WAV file format = "); Serial.println(pCurrentWavData->audio_format);
  Serial.print("WAV number channels = "); Serial.println(pCurrentWavData->num_channels);
  Serial.print("WAV File Sample Rate = "); Serial.println(pCurrentWavData->sample_rate);
  Serial.print("Number of bits per Sample = "); Serial.println(pCurrentWavData->bits);
  Serial.print("File length, seconds = ");
  Serial.println(0.001f*(float32_t)playWav1.lengthMillis(), 3);

  // Simply wait for the file to finish playing.
  while (playWav1.isPlaying())
    {
    Serial.print("RMS ");
    delay(1000);
    if(rms1.available())
        Serial.print(rms1.read(),5);
    Serial.print(" L    R ");
    if(rms2.available())
        Serial.print(rms2.read(),5);
    // Time in seconds
    Serial.print("  t=");
    Serial.println(0.001f*(float32_t)playWav1.positionMillis(), 3);
  }
  Serial.println("");
}

void loop() {
  playFile("SDTEST1.WAV");  // filenames are always uppercase 8.3 format
  delay(500);
  playFile("SDTEST2.WAV");
  delay(500);
  playFile("SDTEST3.WAV");
  delay(500);
  playFile("SDTEST4.WAV");
  delay(500);
}
