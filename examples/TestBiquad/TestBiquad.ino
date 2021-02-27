/*
  TestBiquad.ino  Test the
* F32 library AudioFilterBiquad_F32
* Bob Larkin 26Feb 2021
* Public Domain
*/
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will handle.
const float sample_rate_Hz = 44117.0f ;  // 24000, 44117, or other frequencies listed above (untested)
const int   audio_block_samples = 128;   // Others untested
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);  // Not used

// GUItool: begin automatically generated code
AudioInputI2S_F32        audioInI2S1;    //xy=73.5,556
AudioSynthWaveformSine_F32 sine1;          //xy=89,595
AudioMixer4_F32          mixer4_1;       //xy=263,572
AudioAnalyzeRMS_F32      rms1;           //xy=411,528
AudioFilterBiquad_F32    biquad1;        //xy=414,586
AudioAnalyzeRMS_F32      rms2;           //xy=547,528
AudioOutputI2S_F32       audioOutI2S1;   //xy=576,585
AudioConnection_F32      patchCord1(audioInI2S1, 0, mixer4_1, 0);
AudioConnection_F32      patchCord2(audioInI2S1, 1, mixer4_1, 1);
AudioConnection_F32      patchCord3(sine1, 0, mixer4_1, 2);
AudioConnection_F32      patchCord4(mixer4_1, biquad1);
AudioConnection_F32      patchCord5(mixer4_1, rms1);
AudioConnection_F32      patchCord6(biquad1, 0, audioOutI2S1, 1);
AudioConnection_F32      patchCord7(biquad1, 0, audioOutI2S1, 0);
AudioConnection_F32      patchCord8(biquad1, rms2);
AudioControlSGTL5000     sgtl5000_1;     //xy=327,636
// GUItool: end automatically generated code
// Reminder: Check that AudioMemory_F32 is allocated and that
// settings are added to F32 objects as needed, including Memory.

void setup() {
  Serial.begin(300);
  delay(1000);
  Serial.println("OpenAudio_ArduinoLibrary  - TestBiquad");

  AudioMemory(10);      //allocate Int16 audio data blocks

  AudioMemory_F32(20, audio_settings);

  // Enable the audio shield, select input, and enable output
  sgtl5000_1.enable();                   //start the audio board
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);       // or AUDIO_INPUT_MIC
  sgtl5000_1.volume(0.8);                //volume can be 0.0 to 1.0
  sgtl5000_1.lineInLevel(10, 10);        //level can be 0 to 15.  5 is the Teensy Audio Library's default

  mixer4_1.gain(0, 1.0f);  // Left in
  mixer4_1.gain(1, 1.0f);  // Right In
  mixer4_1.gain(2, 0.0f);  // Sine wave in

  // **** BiQuad Filter Selection  -  Pick one of the next ****
  //biquad1.setHighpass(0, 40.0f, 0.707f);
  //biquad1.setHighpass(0, 1000.0f, 0.707f);
  biquad1.setLowpass(0, 2500.0f, 0.707f);
  //for(int jj=0; jj<4; jj++)  biquad1.setLowpass(jj, 2000.0f, 0.707f);
  //biquad1.setBandpass(0, 1000.0f, 1.5f);
  //biquad1.setNotch(0, 1000.0f, 1.0f);

  // Coeffs, a0=1.0 always     b0               b1             b2              a1              a2
  //double cfLP2500[] = {0.025157343556, 0.050314687112, 0.025157343556, 1.503844064936, -0.604473439160};
  //biquad1.setCoefficients(0, cfLP2500);    // (int iStage, double *cf)  // Load cfLP2500, 1 stage
  // ************************************************************
  biquad1.begin();

  // Download a pointer to the coefficients to see what is happening
  double* pc=biquad1.getCoeffs();
  Serial.println("     b0              b1              b2              a1              a2");
  for(int ii=0; ii<4; ii++)  {
     Serial.print( *(pc + 0 + ii*5),12 );  Serial.print(", ");
     Serial.print( *(pc + 1 + ii*5),12 );  Serial.print(", ");
     Serial.print( *(pc + 2 + ii*5),12 );  Serial.print(", ");
     Serial.print( *(pc + 3 + ii*5),12 );  Serial.print(", ");
     Serial.print( *(pc + 4 + ii*5),12 );  Serial.println();
     }

  sine1.frequency(500.0);  // Connect via   mixer4_1.gain(2, 0.0f);
  sine1.amplitude(0.02);
}

void loop() {
  //update the memory and CPU usage...if enough time has passed
  printMemoryAndCPU(millis());
}

void printMemoryAndCPU(unsigned long curTime_millis) {
  static unsigned long updatePeriod_millis = 2000; //how many milliseconds between updating gain reading?
  static unsigned long lastUpdate_millis = 0;
  float r1, r2;
  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?

  if( rms1.available() )  {
     Serial.print("RMS in: ");
     Serial.print(r1=rms1.read(), 6);
     }
  if( rms2.available() )  {
     Serial.print("  RMS out: ");
     Serial.print(r2=rms2.read(), 6);
     Serial.print("  Gain, dB = ");
     Serial.print( 20.0f*log10f(r2/r1));
     }

  Serial.print("  CPU: Usage, Max: ");
  Serial.print(AudioProcessorUsageMax());
  Serial.print("  Float Memory, Max: ");
  Serial.println(AudioMemoryUsageMax_F32());
  lastUpdate_millis = curTime_millis; //we will use this value the next time around.
  }
}
