// VoiceClipper.ino     Bob Larkin W7PUA
// Uses radioVoiceClipper_F32.h and .cpp.  See the .h file for
// more information and references.  These clippers, when used with
// radio systems, are for AM and FM (or NBFM) modulation, and NOT SSB.
// See the similar class CESSBtransmit_F32 for SSB use.
//
// Tests with voice from SD Card file and a 1 second 750 Hz tone burst.
// Input is from the WAV file on the SD card.  Output is to the Teensy
// Audio Adaptor left and right Line Outputs.
//
// The SD card may connect to different pins, depending on the
// hardware you are using.  Configure the SD card
// pins to match your hardware.  It is set for T4.x Rev D PJRC
// Teensy Audio Adaptor card here.
//
// Your microSD card must have  W9GR12.WAV file loaded to it, found at
// https://github.com/chipaudette/OpenAudio_ArduinoLibrary/blob/master/utility/
//
// ********   This example runs at 48 ksps.     *********
// As of March 2023, this could be adapted to any rate, 11 to 12 or 44 to 50 ksps.
//
// This example code is in the public domain.

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "OpenAudio_ArduinoLibrary.h"
//#include "radioVoiceClipper_F32.h" part of "radioVoiceClipper_F32.h"

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will handle.
const float sample_rate_Hz = 48000.0f;
const int   audio_block_samples = 128;  // Always 128
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

AudioSynthWaveformSine_F32   sine1(audio_settings);
AudioSDPlayer_F32            playWav1(audio_settings);
AudioMixer4_F32              mixer4_0;
radioVoiceClipper_F32        clipper1(audio_settings);
AudioOutputI2S_F32           audioOutput(audio_settings);
AudioAnalyzeFFT1024_F32      fft1;

AudioConnection_F32          patchCord0(playWav1, 0, mixer4_0, 0);
AudioConnection_F32          patchCord1(sine1, 0,    mixer4_0, 1);
AudioConnection_F32          patchCord2(mixer4_0, 0, clipper1, 0);
AudioConnection_F32          patchCord3(clipper1, 0, audioOutput, 0);
AudioConnection_F32          patchCord4(clipper1, 0, audioOutput, 1);
AudioConnection_F32          patchCord5(clipper1, 0, fft1, 0);
AudioControlSGTL5000         sgtl5000_1;

// Use these with the Teensy 4.x Rev D Audio Shield (NOT for T3.x)
#define SDCARD_CS_PIN_Z    10
#define SDCARD_MOSI_PIN_Z  11
#define SDCARD_SCK_PIN_Z   13

// wavData is a global struct, definined in AudioSDPlayer_F32.h
// This provides information about the current WAV file to this .INO
struct wavData* pCurrentWavData;
// And data about the clipper
struct levelClipper* pLevelData;
uint32_t writeOne = 0;
uint32_t cntFFT = 0;
uint32_t ttt;    // For timing test audio

uint32_t tp;

void setup() { //        **********  SETUP  **********
  Serial.begin(9600); delay(1000);
  Serial.println("*** Test Voice Clipper from SD Card Voice Sample ***");

  AudioMemory_F32(70, audio_settings);
  pCurrentWavData = playWav1.getCurrentWavData();
  sgtl5000_1.enable();
  delay(500);
  SPI.setMOSI(SDCARD_MOSI_PIN_Z);
  SPI.setSCK(SDCARD_SCK_PIN_Z);
  Serial.print("SD.begin() returns "); Serial.println(SD.begin(SDCARD_CS_PIN_Z));

  sine1.frequency(750.0f);
  sine1.amplitude(0.707107);

  clipper1.setSampleRate_Hz(48000.0f);
  // Set input, correction, and output gains
  float32_t Pre_Clip_Gain = 1.5f; // Use to set amount of clipping, 1.0 to 2.0f, 3 is excessive
  // Correction gain=1.8 leaves max output overshoot at 2.2% for W9GR sample audio
  clipper1.setGains(Pre_Clip_Gain, 1.8f, 1.0f);
  pLevelData = clipper1.getLevels(0);  // Gets pointer to struct

  audioOutput.setGain(0.02);    // <<< Output volume control
  fft1.setOutputType(FFT_DBFS);
  fft1.windowFunction(AudioWindowBlackmanHarris1024);
  fft1.setNAverage(16);
  ttt = millis();  // Time test audio
  tp=millis();
  }

void playFile(const char *filename)  {
  if(playWav1.isPlaying())
      return;
  Serial.println("");
  Serial.print("Playing file: ");
  Serial.println(filename);
  playWav1.play(filename);        // Start playing the file.
  // A brief delay for the library read WAV info
  delay(25);
  Serial.print("WAV file format = "); Serial.println(pCurrentWavData->audio_format);
  Serial.print("WAV number channels = "); Serial.println(pCurrentWavData->num_channels);
  Serial.print("WAV File Sample Rate = "); Serial.println(pCurrentWavData->sample_rate);
  Serial.print("Number of bits per Sample = "); Serial.println(pCurrentWavData->bits);
  Serial.print("File length, seconds = ");
  Serial.println(0.001f*(float32_t)playWav1.lengthMillis(), 3);
}

void loop() {
  uint32_t tt=millis() - ttt;
  if(tt < 2000)
     {
     // Thanks to W9GR for the test file, W9GR48.WAV.
     playFile("W9GR48.WAV");
     mixer4_0.gain(0, 1.41421356f);  // Play WAV file on
     mixer4_0.gain(1, 0.0f);  // Sine Wave 1 off
     }
  else if(tt > 12300 && tt<13300)
     {
     mixer4_0.gain(1, 0.0f);  // Play WAV file off
     // The following puts a 1-sec 750 Hz, full amplitude tone into the input
     // .707 on the generator and 1.414 here make the peak sine wave 1.000 at the CESSB input
     mixer4_0.gain(1, 1.41421356f);  // Sine Wave 1 on, 750 Hz
     }
  else if(tt >= 13300)
     {
     mixer4_0.gain(0, 1.41421356f);  // Play WAV file on
     mixer4_0.gain(1, 0.0f);  // Sine Wave 1 off
     ttt = millis();         // Start again
     }
  delay(1);

// Un-comment the following to print out the spectrum
/*  if(fft1.available() && ++cntFFT>100 && cntFFT<102)
     {
     for(int kk=0; kk<512; kk++)
        {
        Serial.print(46.875f*(float32_t)kk); Serial.print(",");
        Serial.println(fft1.read(kk));
        }
     }
 */
    if(clipper1.levelDataCount() > 300)  // Typically 300 to 3000
        {
        clipper1.getLevels(1);  // Cause write of data to struct & reset

        // Detailed Report
        Serial.print(10.0f*log10f(pLevelData->pwr0));
        Serial.print(" In Ave Pwr Out ");
        Serial.println(10.0f*log10f(pLevelData->pwr1));
        Serial.print(20.0f*log10f(pLevelData->peak0));
        Serial.print(" In  Peak   Out ");
        Serial.println(20.0f*log10f(pLevelData->peak1));
        Serial.print(pLevelData->peak0, 6);
        Serial.print(" In  Peak Volts   Out ");
        Serial.println(pLevelData->peak1, 6);
        Serial.print("Enhancement = ");
        float32_t enhance = (10.0f*log10f(pLevelData->pwr1) - 20.0f*log10f(pLevelData->peak1)) -
                            (10.0f*log10f(pLevelData->pwr0) - 20.0f*log10f(pLevelData->peak0));
        if(enhance<1.0f) enhance = 1.0f;
        Serial.print(enhance); Serial.println(" dB");

/*
        // CSV Report suitable for entering to spread sheet
        // InAve, InPk, OutAve, OutPk, EnhancementdB
        Serial.print(pLevelData->pwr0, 5);  Serial.print(",");
        Serial.print(pLevelData->peak0, 5); Serial.print(",");
        Serial.print(pLevelData->pwr1, 5);  Serial.print(",");
        Serial.print(pLevelData->peak1, 5); Serial.print(",");
        float32_t enhance = (10.0f*log10f(pLevelData->pwr1) - 20.0f*log10f(pLevelData->peak1)) -
                            (10.0f*log10f(pLevelData->pwr0) - 20.0f*log10f(pLevelData->peak0));
        if(enhance<1.0f) enhance = 1.0f;
        Serial.println(enhance);
 */
       }
    }
