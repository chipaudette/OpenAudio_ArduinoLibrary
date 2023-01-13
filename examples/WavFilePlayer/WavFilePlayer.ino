// WavFilePlayer.ino
//
// WAV file player, mono, multi-rate example
// Output is Digital I2S - Used with the audio shield:
//
// The SD card may connect to different pins, depending on the
// hardware you are using.  Configure the SD card
// pins to match your hardware.  It is set for T4.x Rev D PJRC
// Teensy Audio Adaptor card here.
//
// Your microSD card must have the WAV files loaded to it:
//    W9GR48.WAV
//    W9GR12.WAV
// These are at
// https://github.com/chipaudette/OpenAudio_ArduinoLibrary/blob/master/utility/
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
// This example is monoraural only, but demonstrates 12 ksps sampling
// in the WAV file.  See the other example, wavPlayerStereo.ino
// that uses stereo files, like the original I16 library
// example did.
// This example code is in the public domain.

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "OpenAudio_ArduinoLibrary.h"

//  This INO is setup to run audio sample rate at 48 ksps but the WAV file
// can be either 48 ksps or 12 ksps.  There are multiple W9GR files at different
// sample rates.  Select by SUB_MULT of 1 or 4.
#define SUB_MULT 1

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will hanedle.
const float sample_rate_Hz = 48000.0f;
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

#if SUB_MULT==4
/* This filter is a LPF to smooth out the zero data added for interpolation
 * of the sample rate.  It is a
 * FIR filter designed with http://t-filter.appspot.com
 * Sampling frequency: 48000 Hz
 * 0 Hz - 3000 Hz ripple = 0.12 dB
 * 6000 Hz - 24000 Hz atten min = -62.17 dB  */
static float32_t fir3k48k[45] = {
-0.00111325f,-0.00202997f,-0.00306939f,-0.00351866f,-0.00274389f,-0.00035614f,
 0.00346572f, 0.00778335f, 0.01098197f, 0.01119770f, 0.00702222f,-0.00172373f,
-0.01344104f,-0.02471188f,-0.03091211f,-0.02741134f,-0.01105157f, 0.01854769f,
 0.05833902f, 0.10214361f, 0.14192191f, 0.16971322f, 0.17968923f, 0.16971322f,
 0.14192191f, 0.10214361f, 0.05833902f, 0.01854769f,-0.01105157f,-0.02741134f,
-0.03091211f,-0.02471188f,-0.01344104f,-0.00172373f, 0.00702222f, 0.01119770f,
 0.01098197f, 0.00778335f, 0.00346572f,-0.00035614f,-0.00274389f,-0.00351866f,
-0.00306939f,-0.00202997f,-0.00111325f};

// CMSIS FIR requires the following buffers
float32_t firBufferL[128+45-1];
float32_t firBufferR[128+45-1];
#endif

/* subMult is a global struct definition from AudioSDPlayer_F32.h
 * struct subMult {
 *   uint16_t   rateRatio;   // Should be 1 for no rate change, else 2, 4, 8
 *   uint16_t   numCoeffs;   // FIR filter for interpolation
 *   float32_t* firCoeffs;   // FIR Filter Coeffs
 *   float32_t* firBufferL;  // pointer to 127 + numCoeffs float32_t, left ch
 *   float32_t* firBufferR;  // pointer to 127 + numCoeffs float32_t, right ch
 *   };
 */
#if SUB_MULT==4
// Both buffers are left for mono.
struct subMult wavQuarter = {4, 45, fir3k48k, firBufferL, firBufferL};
// Next a sample of subMult for no interpolation and no FIR filter
#else
struct subMult wavQuarter = {1, 0, NULL, NULL, NULL};
#endif
// And next is a sample of no interpolation, but borrowing the FIR filter
// for  some non-interpolation reason.
//   struct subMult wavQuarter = {1, 45, fir3k48k, firBufferL, firBufferL};

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
  Serial.print("SD.begin() returns "); Serial.println(SD.begin(SDCARD_CS_PIN));
}

void playFile(const char *filename)
{
  Serial.println("");
  Serial.print("Playing file: ");
  Serial.println(filename);

  // Allow for running the WAV file at a sub-rate from the Audio rate
  // Two of these are defined above for 1 and 4 sub rates.
  playWav1.setSubMult(&wavQuarter);

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
  // Thanks to W9GR for the test files.  These are intended for testing
  // the CESSB radio transmission system (see CESSB1.ino in the examples)
  // but work well to provide a voice bandwidth test of WAV file reading.
#if SUB_MULT==4
  playFile("W9GR12.WAV");  // filenames are always uppercase 8.3 format
#elif SUB_MULT==1
  playFile("W9GR48.WAV");
#else
  Serial.println("**** Non-used SUB_MULT.  Use 1 or 4. ****");
  #endif
  delay(500);
}
