/* Test AudioLMSDenoiseNotch_F32 from OpenAudio_ArduinoLibrary
 *  Just a simpe sine wave plus noise input.  Select
 *  Deoise or AutoNotch Filter functions by #defines below.
 *  Output is either a sample of the time series or
 *  the spectrum fro the FFT.
 *
 * For notes, see: AudioLMSDenoiseNotch_F32.h
 * 
 * Bob Larkin 29 Jan 2022
 * Public Domain
 */
#include "AudioStream_F32.h"
#include "Arduino.h"
#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"

// ***** UN-COMMENT ONE:
#define DO_DENOISE
//#define DO_AUTONOTCH

// ***** UN-COMMENT ONE:
#define OUTPUT_QUEUE
// #define OUTPUT_FFT

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will handle.

const float sample_rate_Hz = 44117.0f;
const int   audio_block_samples = 128;
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include <Audio.h>

AudioSynthSineCosine_F32 sine1;      //xy=60,219
AudioSynthGaussian_F32   GaussianWhiteNoise1; //xy=106,263
AudioMixer4_F32          mixer4_1;       //xy=154,334
AudioLMSDenoiseNotch_F32 LMS1;           //xy=206,411
AudioOutputI2S_F32       audioOutI2S1;   //xy=301,542
AudioAnalyzeFFT1024_F32  FFT1024_1;      //xy=362,495
AudioRecordQueue_F32     recordQueue1;   //xy=378,446
AudioConnection_F32      patchCord1(sine1, 0, mixer4_1, 0);
AudioConnection_F32      patchCord2(GaussianWhiteNoise1, 0, mixer4_1, 1);
AudioConnection_F32      patchCord3(mixer4_1, LMS1);
AudioConnection_F32      patchCord4(LMS1, recordQueue1);
AudioConnection_F32      patchCord5(LMS1, FFT1024_1);
AudioConnection_F32      patchCord6(LMS1, 0, audioOutI2S1, 0);
AudioConnection_F32      patchCord7(LMS1, 0, audioOutI2S1, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=97,571

float saveDat[512];

void setup() {
  Serial.begin(300);  // Any value
  delay(1000);
  Serial.println("OpenAudio_ArduinoLibrary  - Test LMS DeNoise & AutoNotch");
  
  // AudioMemory(5);
  AudioMemory_F32(50, audio_settings);
  sgtl5000_1.enable();
  // Change the next 3 to suit your experiment
  sine1.frequency(1292.49f);
  sine1.amplitude(0.5f);
  GaussianWhiteNoise1.amplitude(0.1f);  // Standard Deviation

  Serial.print("Achieved FIR length = ");
#ifdef DO_DENOISE
  // Change the last two parameters for array sizes.  See .h file.
  Serial.println(LMS1.initializeLMS(DENOISE, 64, 4));  // <== Modify to suit
#endif
#ifdef DO_AUTONOTCH
  Serial.println(LMS1.initializeLMS(NOTCH, 32, 4));  // <== Modify to suit
#endif
  LMS1.setParameters(0.05f, 0.999f);      // (float _beta, float _decay);
  LMS1.enable(true);
  FFT1024_1.windowFunction(NULL);     // (AudioWindowHanning1024);
  
#ifdef OUTPUT_QUEUE
  recordQueue1.begin();
#endif
  }

void loop()
  {
  float *pq;
  int nQ;
  static uint32_t nTimes = 0;
#ifdef OUTPUT_FFT
  if ( FFT1024_1.available() )
     {
     // When new FFT data is available
     // print it all to the Arduino Serial Monitor
     float* pin = FFT1024_1.getData();
     for (int  kk=0; kk<512; kk++)
         saveDat[kk]= *(pin + kk);
     if(++nTimes>4 && nTimes<6)
         {
         Serial.println("Freq, Hz   Power, dB");
         for (int i=0; i<512; i++)
             {
             Serial.print(43.083f*(float32_t)i, 2);
             Serial.print(",  ");
             Serial.println(20.0f*log10f(0.0078125f*saveDat[i]), 3);
             }
         Serial.println();
         }
      }
#endif

#ifdef OUTPUT_QUEUE
  if( nQ = recordQueue1.available() )
    {// Serial.print("Data Available ");
      pq = recordQueue1.readBuffer();
      for(int i=0; i<128; i++)
         Serial.println(*(pq + i),7);
      recordQueue1.freeBuffer();
    }  
#endif
  }
