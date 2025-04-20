/*
 * Very basic demo of 24-bit TDM I/O, using
 * 6i8o CS42448 board.
 * 
 * Suggested wiring is to monitor outputs 
 * 1, 2, 5 and 6, and connect output 3 to
 * input 1.
 * 
 * Output 3: 441Hz, so input 1 has that at half amplitude.
 * Output 1: 440Hz sine
 * Output 2: 440Hz cosine
 * Output 5: copy of input 1
 * Output 6: 1x input 1 + 0.5x sine (1Hz beat)
 * 
 * Input 1 has an RMS object so you can check it's active.
 * 
 * Scope capture has outputs 1+2 in blue and green, 
 * output 5 in magenta, output 6 in yellow.
 */
#include <Audio.h>

//======================================================
// Integer audio library objects:

// GUItool: begin automatically generated code
AudioInputTDM            tdmI;           //xy=572,233
AudioOutputTDM           tdmO;           //xy=708,237


AudioControlCS42448      cs42448;        //xy=699,387
// GUItool: end automatically generated code

//======================================================
// F32 audio library objects
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"

const float sample_rate_Hz = 44100.0f ;  
const int   audio_block_samples = 128;    // Always 128, which is AUDIO_BLOCK_SAMPLES from AudioStream.h
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

// GUItool: begin automatically generated code
//AudioInputI2S_F32        dummyI2S;    //xy=302,461 // dummy to allow export
AudioConvert_I16x2toF32  convert_I16x2toF32_1; //xy=348,511
AudioAnalyzeRMS_F32      rms;           //xy=548,570
AudioSynthSineCosine_F32 sine_cos1;      //xy=598,333
AudioSynthWaveform_F32   wav1;      //xy=605,395
AudioMixer4_F32          mixer4;       //xy=644,490
AudioConvert_F32toI16x2  convert_F32toI16x2_1; //xy=812,314
AudioConvert_F32toI16x2  convert_F32toI16x2_2; //xy=820,352
AudioConvert_F32toI16x2  convert_F32toI16x2_3; //xy=829,395
AudioConvert_F32toI16x2  convert_F32toI16x2_5; //xy=852,490
AudioConvert_F32toI16x2  convert_F32toI16x2_6; //xy=862,540
AudioConnection_F32          patchCord1(convert_I16x2toF32_1, convert_F32toI16x2_6);
AudioConnection_F32          patchCord2(convert_I16x2toF32_1, 0, mixer4, 1);
AudioConnection_F32          patchCord3(convert_I16x2toF32_1, rms);
AudioConnection_F32          patchCord4(sine_cos1, 0, convert_F32toI16x2_1, 0);
AudioConnection_F32          patchCord5(sine_cos1, 0, mixer4, 0);
AudioConnection_F32          patchCord6(sine_cos1, 1, convert_F32toI16x2_2, 0);
AudioConnection_F32          patchCord7(wav1, convert_F32toI16x2_3);
AudioConnection_F32          patchCord8(mixer4, convert_F32toI16x2_5);
// GUItool: end automatically generated code

//======================================================
// Hand-generated patchcords between libraries
AudioConnection pc1(tdmI,0, convert_I16x2toF32_1,0);
AudioConnection pc2(tdmI,1, convert_I16x2toF32_1,1);

AudioConnection  pc3(convert_F32toI16x2_1,0,  tdmO, 0);
AudioConnection  pc4(convert_F32toI16x2_1,1,  tdmO, 1);
AudioConnection  pc5(convert_F32toI16x2_2,0,  tdmO, 2);
AudioConnection  pc6(convert_F32toI16x2_2,1,  tdmO, 3);
AudioConnection  pc7(convert_F32toI16x2_3,0,  tdmO, 4);
AudioConnection  pc8(convert_F32toI16x2_3,1,  tdmO, 5);
// TDM output 4 (ports 6+7) unused
AudioConnection  pc9(convert_F32toI16x2_5,0,  tdmO, 8);
AudioConnection pc10(convert_F32toI16x2_5,1,  tdmO, 9);
AudioConnection pc11(convert_F32toI16x2_6,0,  tdmO,10);
AudioConnection pc12(convert_F32toI16x2_6,1,  tdmO,11);

//======================================================
void setup() 
{
  AudioMemory(48); // TDM uses many blocks!
  AudioMemory_F32(20);

  cs42448.enable();
  cs42448.volume(0.5f);
  cs42448.inputLevel(0.5f);

  pinMode(LED_BUILTIN,OUTPUT);
  mixer4.gain(0,0.5);
  mixer4.gain(1,1.0); // input level is set to 0.5, so boost
  
  sine_cos1.amplitude(0.5f); 
  sine_cos1.frequency(440.0f);

  wav1.begin(0.5f,441.0f,WAVEFORM_SINE);

  // Un-comment for low output high gain test. Note
  // that the level is extremely low, so there will be
  // essentially no visible signal on output 5!
  /*
  float attenuation = 2000.0f;
  wav1.begin(1.0f/attenuation,441.0f,WAVEFORM_SINE);
  mixer4.gain(1,attenuation/2); 
  //*/
}


void loop() 
{
  delay(250);
  digitalToggle(LED_BUILTIN);
  if (rms.available())
  {
    Serial.printf("RMS input level: %.3f; audio memory: %d; F32 audio memory: %d\n", 
                rms.read(), AudioMemoryUsageMax(), AudioStream_F32::f32_memory_used_max);
    AudioMemoryUsageMaxReset();                
    AudioMemoryUsageMaxReset_F32();                
  }
}
