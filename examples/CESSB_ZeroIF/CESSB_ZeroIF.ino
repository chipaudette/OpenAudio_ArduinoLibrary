// CESSB_ZeroIF.ino
// This tests the Controlled Envelope Single Sideband generator version
// that produces a zero-IF signal 0to 3kHz or 0 to -3 kHz.
// Uses radioCESSB_Z_transmit_F32.h and .cpp.  See the .h file for
// more information and references.
// Tests with voice from SD Card file and a 1 second 750 Hz tone burst.
//
// The SD card may connect to different pins, depending on the
// hardware you are using.  Configure the SD card
// pins to match your hardware.  It is set for T4.x Rev D PJRC
// Teensy Audio Adaptor card here.
//
// Your microSD card must have the WAV file loaded to it:
//    W9GR48.WAV
// These are at
// https://github.com/chipaudette/OpenAudio_ArduinoLibrary/blob/master/utility/
//
// This example code is in the public domain.

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "OpenAudio_ArduinoLibrary.h"

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will handle.
const float sample_rate_Hz = 48000.0f;
const int   audio_block_samples = 128;  // Always 128
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

AudioSynthWaveformSine_F32 sine1(audio_settings);
AudioSDPlayer_F32          playWav1(audio_settings);
AudioMixer4_F32            mixer4_0;
radioCESSB_Z_transmit_F32  cessb1(audio_settings);
RadioIQMixer_F32           iqMixer1(audio_settings);
AudioMixer4_F32            mixer4_2;
AudioFilter90Deg_F32       filter90deg1(audio_settings);
RadioIQMixer_F32           iqMixer2(audio_settings);
AudioMixer4_F32            mixer4_1;
AudioOutputI2S_F32         audioOutput(audio_settings);
AudioAnalyzeFFT1024_F32    fft1;

AudioConnection_F32          patchCord0(playWav1, 0, mixer4_0, 0);
AudioConnection_F32          patchCordb(sine1, 0,    mixer4_0, 1);
AudioConnection_F32          patchCordc(mixer4_0, 0, cessb1, 0);
AudioConnection_F32          patchCord1(cessb1,   0, iqMixer1, 0);
AudioConnection_F32          patchCord2(cessb1,   1, iqMixer1, 1);
AudioConnection_F32          patchCord9(iqMixer1, 0, mixer4_2, 0);
AudioConnection_F32          patchCord10(iqMixer1,1, mixer4_2, 1);

// mixer4_2 is transmitter SSB output, iqMixer2 is receiver input
AudioConnection_F32          patchCord14(mixer4_2,0, iqMixer2, 0);
AudioConnection_F32          patchCord3(iqMixer2, 0, filter90deg1, 0);
AudioConnection_F32          patchCord4(iqMixer2, 1, filter90deg1, 1);
AudioConnection_F32          patchCord7(filter90deg1, 0, mixer4_1, 0);
AudioConnection_F32          patchCord8(filter90deg1, 1, mixer4_1, 1);
AudioConnection_F32          patchCord11(mixer4_1, 0, audioOutput, 0);
AudioConnection_F32          patchCord12(mixer4_1, 0, audioOutput, 1);
AudioConnection_F32          patchCord13(mixer4_2, 0, fft1, 0);

AudioControlSGTL5000         sgtl5000_1;

// Use these with the Teensy 4.x Rev D Audio Shield (NOT for T3.x)
#define SDCARD_CS_PIN_Z    10
#define SDCARD_MOSI_PIN_Z  11
#define SDCARD_SCK_PIN_Z   13

// Filter for AudioFilter90Deg_F32 hilbert1, only for receiving the CESSB
#include "hilbert251A_Z_.h"

// wavData is a global struct, definined in AudioSDPlayer_F32.h
// This provides information about the current WAV file to this .INO
struct wavData* pCurrentWavData;
// And data about the CESSB
struct levelsZ* pLevelData;
uint32_t writeOne = 0;
uint32_t cntFFT = 0;
uint32_t ttt;    // For timing test audio

uint32_t tp;

void setup() { //        **********  SETUP  **********
  Serial.begin(9600); delay(1000);
  Serial.println("*** Test CESSB Zero-IF from SD Card Voice Sample ***");

  AudioMemory_F32(70, audio_settings);
  pCurrentWavData = playWav1.getCurrentWavData();
  sgtl5000_1.enable();
  delay(500);
  SPI.setMOSI(SDCARD_MOSI_PIN_Z);
  SPI.setSCK(SDCARD_SCK_PIN_Z);
  Serial.print("SD.begin() returns "); Serial.println(SD.begin(SDCARD_CS_PIN_Z));

//  sine0.frequency(468.75f);    // 2-tone generators
//  sine0.amplitude(0.707107);
  sine1.frequency(750.0f);
  sine1.amplitude(0.707107);

  //   Build the CESSB SSB transmitter
  // The WAV file has carefully controlled 0.707 peaks. We bring these to 1.000
  mixer4_0.gain(0, 1.41421356f);  // Play WAV file on
  mixer4_0.gain(2, 0.0);  // Sine Wave 1 off

  cessb1.setSampleRate_Hz(48000.0f);
  // Set input, correction, and output gains
  float32_t Pre_CESSB_Gain = 1.5f; // Use to set amount of clipping, 1.0 to 2.0f, 3 is excessive
  cessb1.setGains(Pre_CESSB_Gain, 1.4f, 1.0f);
  cessb1.setSideband(false);
  pLevelData = cessb1.getLevels(0);  // Gets pointer to struct

  // Generate SSB at 15 kHz from zero-IF signal of CESSB generator
  iqMixer1.useTwoChannel(true);
  iqMixer1.frequency(15000);
  mixer4_2.gain(1,  1.0f);      // 1.0f for LSB,  -1.0f for USB

  // Need a receiver for the SSB transmitter to let us hear the results.
  iqMixer2.frequency(15000.0f);
  iqMixer2.useTwoChannel(false);
  filter90deg1.begin(hilbert251A, 251);
  mixer4_1.gain(0, -1.0f); // LSB, + for USB
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
     // Thanks to W9GR for the test file, W9GR48.WAV.  This is intended for testing
     // the CESSB radio transmission system.
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
/*
  if(fft1.available() && ++cntFFT>100 && cntFFT<102)
     {
     for(int kk=0; kk<512; kk++)
        {
        Serial.print(46.875f*(float32_t)kk); Serial.print(",");
        Serial.println(fft1.read(kk));
        }
     }
 */

    if(cessb1.levelDataCount() > 300)  // Typically 300 to 3000
        {
        cessb1.getLevels(1);  // Cause write of data to struct & reset

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
