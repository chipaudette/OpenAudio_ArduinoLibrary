/*
 * SDWavPlayer
 *
 * Created: Chip Audette, OpenAudio, Dec 2019
 * Based On: WaveFilePlayer from Paul Stoffregen, PJRC, Teensy
 *
 * Play back a WAV file
 *
 * This is the basic WAV file player, adapted from the Tympan library.
 * See also WAVFilePlayer and WAVFilePlayer2     Bob
 *
 * For access to WAV files, please visit https://www.pjrc.com/teensy/td_libs_AudioDataFiles.html.
 *
 */

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioSDPlayer_F32.h"

//set the sample rate and block size
const float sample_rate_Hz = 44100.0f;
const int audio_block_samples = 128;         //   Must be 128 for SD recording.
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

//create audio objects
AudioSDPlayer_F32        audioSDPlayer(audio_settings);
AudioOutputI2S_F32       audioOutput(audio_settings);

//create audio connections
AudioConnection_F32      patchCord1(audioSDPlayer, 0, audioOutput, 0);
AudioConnection_F32      patchCord2(audioSDPlayer, 1, audioOutput, 1);
AudioControlSGTL5000     sgtl5000_1;

// Use these with the Teensy 4.x Rev D Audio Shield
#define SDCARD_CS_PIN   10
#define SDCARD_MOSI_PIN 11
#define SDCARD_SCK_PIN  13

void setup() {
  Serial.begin(300); delay(1000);
  Serial.println("###  SDWavPlayer  ###");
  Serial.print("Audio Sample Rate (Hz): ");
  Serial.println(audio_settings.sample_rate_Hz);
  Serial.print("Audio Block Size (samples): ");
  Serial.println(audio_settings.audio_block_samples);

  // Audio connections require memory to work.
  AudioMemory_F32(20, audio_settings);

  sgtl5000_1.enable();
  audioOutput.setGain(0.05);  // Volume control

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("*** Unable to access the SD card  ***");
      delay(1000);
    }
  }
  //prepare SD player
  audioSDPlayer.begin();

  //finish setup
  delay(1000);  //stall a second
  Serial.println("Setup complete.");
}

void loop() {
  //service the audio player
  if (!audioSDPlayer.isPlaying())
     { //wait until previous play is done
     //start playing audio
     Serial.println("Starting audio player: SDTEST1.WAV");
     audioSDPlayer.play("SDTEST1.WAV");
     }
  delay(500);
}
