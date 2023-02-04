/* CESSB1.ino
 *
 * This INO tests the Dave Hershberger Controlled Envelope Single Sideband
 * transmit generator radioCESSBtransmit_F32. This was developed By W9GR
 * with the aim to "allow your rig to output more average power while
 * keeping peak envelope power PEP the same". The increase in perceived
 * loudness can be up to 4dB without any audible increase in distortion
 * and without making you sound "processed."  See radioCESSBTransmit_F32.h
 * for more information and references.
 *
 * Your microSD card must have the WAV file W9GR48.WAV loaded to it.  Source:
 * https://github.com/chipaudette/OpenAudio_ArduinoLibrary/blob/master/utility/
 *
 * The SD card may connect to different pins, depending on the
 * hardware you are using.  Configure the SD card
 * pins to match your hardware.  It is set for T4.x Rev D PJRC
 * Teensy Audio Adaptor card here.
 *
 * This example code is in the public domain.
 */

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "OpenAudio_ArduinoLibrary.h"

// Normal input is from the WAV file with W9GR voice.
// An alternative is two tones. Compile that with the following define.
// #define USE_TWO_TONE

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will handle.
const float sample_rate_Hz = 48000.0f;
const int   audio_block_samples = 128;  // Always 128, which is AUDIO_BLOCK_SAMPLES from AudioStream.h
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

#ifdef USE_TWO_TONE
AudioSynthWaveformSine_F32 sine3(audio_settings);     //xy=231,444
AudioSynthWaveformSine_F32 sine4(audio_settings);     //xy=259,345
AudioMixer4_F32            mixer4_4;                  //xy=405,427
#endif
AudioSDPlayer_F32          playWav1(audio_settings);    //xy=111,51
radioCESSBtransmit_F32     cessb1(audio_settings);
RadioIQMixer_F32           iqMixer2(audio_settings);     //xy=100,300
AudioFilter90Deg_F32       filter90deg1(audio_settings); //xy=250,300
RadioIQMixer_F32           iqMixer1(audio_settings);     //xy=250,150
AudioAnalyzeRMS_F32        rms1;                         //xy=300,50
AudioMixer4_F32            mixer4_1;                     //xy=400,310
AudioMixer4_F32            mixer4_2;                     //xy=400,150
AudioOutputI2S_F32         audioOutput(audio_settings);  //xy=520,250
AudioAnalyzeFFT1024_F32    fft1;                         //xy=550,125

#ifdef USE_TWO_TONE
AudioConnection_F32          patchCorda(sine3, 0, mixer4_4, 1);
AudioConnection_F32          patchCordb(sine4, 0, mixer4_4, 0);
AudioConnection_F32          patchCordc(mixer4_4, 0, cessb1, 0);
#else
AudioConnection_F32          patchCord5(playWav1, 0, cessb1, 0);
#endif
AudioConnection_F32          patchCord1(cessb1, 0, iqMixer1, 0);
AudioConnection_F32          patchCord2(cessb1, 1, iqMixer1, 1);
AudioConnection_F32          patchCord9(iqMixer1, 0, mixer4_2, 0);
AudioConnection_F32          patchCord10(iqMixer1, 1, mixer4_2, 1);
AudioConnection_F32          patchCord13(mixer4_2, fft1);    // Transmitter output
// mixer4_2 is transmitter SSB output, iqMixer2 is receiver input
AudioConnection_F32          patchCord14(mixer4_2, 0, iqMixer2, 0);
AudioConnection_F32          patchCord3(iqMixer2, 0, filter90deg1, 0);
AudioConnection_F32          patchCord4(iqMixer2, 1, filter90deg1, 1);
AudioConnection_F32          patchCord7(filter90deg1, 0, mixer4_1, 0);
AudioConnection_F32          patchCord8(filter90deg1, 1, mixer4_1, 1);
AudioConnection_F32          patchCord11(mixer4_1, 0, audioOutput, 0);
AudioConnection_F32          patchCord12(mixer4_1, 0, audioOutput, 1);
AudioControlSGTL5000         sgtl5000_1;                     //xy=582,327

// Use these with the Teensy 4.x Rev D Audio Shield (NOT for T3.x)
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  11
#define SDCARD_SCK_PIN   13

// Filter for AudioFilter90Deg_F32 hilbert1, for the test receiver.
#include "hilbert251A.h"

// This provides information about the current WAV file to this .INO
struct wavData* pCurrentWavData;
struct levels* pLevelData;
uint32_t writeOne = 0;

uint32_t cntFFT = 0;

void setup() {       //        **********  SETUP  **********
  Serial.begin(9600); delay(1000);
  Serial.println("*** F32 WAV from SD Card  ***");
  AudioMemory_F32(70, audio_settings);

  pCurrentWavData = playWav1.getCurrentWavData();

  sgtl5000_1.enable();

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  Serial.print("SD.begin() returns ");
  Serial.println(SD.begin(SDCARD_CS_PIN));

#ifdef USE_TWO_TONE
  // Two-tone test, multiples of 48000/1024 Hz
  sine3.frequency(468.75f);
  sine3.amplitude(0.5f);

  sine4.frequency(703.125f);
  sine4.amplitude(0.5f);
#endif

  //   Build the CESSB SSB transmitter
  cessb1.setSampleRate_Hz(48000.0f);  // Required
  // Set input, correction, and output gains
  float32_t Pre_CESSB_Gain = 1.5f; // Sets the amount of clipping, 1.0 to 2.0f, 3 is excessive
  cessb1.setGains(Pre_CESSB_Gain, 2.0f, 1.0f);
  cessb1.setSideband(false);   // true reverses the sideband
  pLevelData = cessb1.getLevels(0);  // Gets pointer to struct

  // Generate SSB at 15 kHz
  iqMixer1.useTwoChannel(true);
  iqMixer1.useSimple(false);  // enables setGainOut()
  // Following if cessb1.setSideband(false), otherwise reverse

  // Compensate for window loss in FFT as well as x2 for mixers
  iqMixer1.setGainOut(2.76f);
  iqMixer1.frequency(13650);   // 13650 for LSB,  16350 for USB
  mixer4_2.gain(1,  1.0f);      // 1.0f for LSB,  -1.0f for USB

  // A receiver for the SSB transmitter
  iqMixer2.frequency(15000.0f);
  iqMixer2.useTwoChannel(false);

  filter90deg1.begin(hilbert251A, 251);  // Set the Hilbert transform FIR filter
  mixer4_1.gain(0, -1.0f); // LSB + for USB
  audioOutput.setGain(0.05);    // <<< Output volume control
  fft1.setOutputType(FFT_DBFS);
  fft1.windowFunction(AudioWindowBlackmanHarris1024);
  fft1.setNAverage(16);
}

void playFile(const char *filename)
{

  if(playWav1.isPlaying())
      return;
  Serial.println("");
  Serial.print("Playing file: ");
  Serial.println(filename);
  // Allow for running the WAV file at a sub-rate from the Audio rate
  // Two of these are defined above for 1 and 4 sub rates.
//  playWav1.setSubMult(&wavQuarter);
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
}

void loop() {
  // Thanks to W9GR for the test files.  These are intended for testing
  // the CESSB radio transmission system (see CESSB1.ino in the examples)
  // but work well to provide a voice bandwidth test of WAV file reading.
#ifndef USE_TWO_TONE
  playFile("W9GR48.WAV");  // filenames are always uppercase 8.3 format
#endif

 if(fft1.available() && ++cntFFT>100 && cntFFT<102)
   {
    for(int kk=0; kk<512; kk++)
      {
      Serial.print(46.875f*(float32_t)kk); Serial.print(",");
      Serial.println(fft1.read(kk));
      }
    }

    if(cessb1.levelDataCount() > 300)  // Typically 300 to 3000
        {
        cessb1.getLevels(1);   // Cause write of data to struct & reset

/*      // Detailed Report  Uncomment to print
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
        Serial.print((10.0f*log10f(pLevelData->pwr1) - 20.0f*log10f(pLevelData->peak1)) -
                     (10.0f*log10f(pLevelData->pwr0) - 20.0f*log10f(pLevelData->peak0)) );
        Serial.println(" dB");
 */

/*
        // CSV Report suitable for entering to spread sheet, Uncomment to print
        // InAve, InPk, OutAve, OutPk, EnhancementdB
        Serial.print(pLevelData->pwr0, 5);  Serial.print(",");
        Serial.print(pLevelData->peak0, 5); Serial.print(",");
        Serial.print(pLevelData->pwr1, 5);  Serial.print(",");
        Serial.print(pLevelData->peak1, 5); Serial.print(",");
        Serial.println((10.0f*log10f(pLevelData->pwr1) - 20.0f*log10f(pLevelData->peak1)) -
                       (10.0f*log10f(pLevelData->pwr0) - 20.0f*log10f(pLevelData->peak0)) );
 */
        }
    } // End loop()
