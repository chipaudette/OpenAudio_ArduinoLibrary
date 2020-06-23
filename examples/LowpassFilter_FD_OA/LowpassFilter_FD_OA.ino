/* LowpassFilter_FD.ino
 *
 * Demonstrate audio procesing in the frequency domain.
 *
 * Created: Chip Audette  Sept-Oct 2016 for Tympan Library
 * Approach:
 *   Take samples in the time domain
 *   Take FFT to convert to frequency domain
 *   Manipulate the frequency bins as desired (LP filter?  BP filter?  Formant shift?)
 *   Take IFFT to convert back to time domain
 *   Send samples back to the audio interface
 *
 * Assumes the use of the Audio library from PJRC
 *
 * Adapted to OpenAudio, "_OA".  June 2020 Bob Larkin.
 * This changed from direct F32 AudioInputI2S to I16 Teensy Audio Library
 * versions with Chip Audette's AudioConvert objects. Also removed volume
 * control. Class and file names are isolated from other libraries by "_OA".
 * Tested T3.6 and T4.0 with PJRC Teensy Audio Adaptor.
 * 
 * Ref: https://github.com/Tympan/Tympan_Library
 *      https://github.com/Tympan/Tympan_Library/tree/master/examples/04-FrequencyDomain/LowpassFilter_FD
 *      https://forum.pjrc.com/threads/40188-Fast-Convolution-filtering-in-floating-point-with-Teensy-3-6
 *      https://forum.pjrc.com/threads/40590-Teensy-Convolution-SDR-(Software-Defined-Radio)
 * 
 * This example code is in the public domain (MIT License)
 */
 
#include "AudioStream_F32.h"
#include <OpenAudio_ArduinoLibrary.h>
#include "AudioEffectLowpassFD_OA_F32.h"  // the local file holding your custom function

//set the sample rate and block size
const float sample_rate_Hz = 44117.f;
const int audio_block_samples = 128;     // for freq domain processing, choose 16, 32, 64 or 128
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

//create audio library objects for handling the audio
AudioInputI2S              i2sIn;
AudioConvert_I16toF32      cnvrt1;
AudioSynthWaveformSine_F32 sinewave(audio_settings);
AudioEffectLowpassFD_F32   audioEffectLowpassFD(audio_settings);  //create the frequency-domain processing block
AudioConvert_F32toI16      cnvrt2; 
AudioOutputI2S             i2sOut;
AudioControlSGTL5000       codec;
//Make all of the audio connections if 1 for Codec input, 0 for internally generated sine wave
#if 1
  AudioConnection          patchCord1(i2sIn,    0, cnvrt1,               0); // connect to Left codec, 16-bit
  AudioConnection_F32      patchCord2(cnvrt1,   0, audioEffectLowpassFD, 0); // Now converted to float
#else
  AudioConnection_F32      patchCord1(sinewave, 0, audioEffectLowpassFD, 0); // connect sine to filter
#endif
AudioConnection_F32        patchCord5(audioEffectLowpassFD, 0,   cnvrt2, 0);  // filtered output
AudioConnection            patchCord6(cnvrt2,   0, i2sOut,               0);  // Left channel

int is_windowing_active = 0;
void setup() {
 //begin the serial comms (for debugging), any baud rate
  Serial.begin(1);  delay(500);
  Serial.println("FrequencyDomainDemo2: starting setup()...");
  Serial.print("    : sample rate (Hz) = ");  Serial.println(audio_settings.sample_rate_Hz);
  Serial.print("    : block size (samples) = ");  Serial.println(audio_settings.audio_block_samples);

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(10); AudioMemory_F32(20, audio_settings);

  codec.enable();
  
  // Configure the frequency-domain algorithm
  int N_FFT = 1024;
  audioEffectLowpassFD.setup(audio_settings,N_FFT); //do after AudioMemory_F32();

  sinewave.frequency(1000.0f);
  sinewave.amplitude(0.025f);

  Serial.println("Setup complete.");
}

void loop() {

} 

//This routine prints the current and maximum CPU usage and the current usage of the AudioMemory that has been allocated
void printCPUandMemory(unsigned long curTime_millis, unsigned long updatePeriod_millis) {
  //static unsigned long updatePeriod_millis = 3000; //how many milliseconds between updating gain reading?
  static unsigned long lastUpdate_millis = 0;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?
    Serial.print("printCPUandMemory: ");
    Serial.print("CPU Cur/Peak: ");
    Serial.print(audio_settings.processorUsage());
    //Serial.print(AudioProcessorUsage()); //if not using AudioSettings_F32
    Serial.print("%/");
    Serial.print(audio_settings.processorUsageMax());
    //Serial.print(AudioProcessorUsageMax());  //if not using AudioSettings_F32
    Serial.print("%,   ");
    Serial.print("Dyn MEM Int16 Cur/Peak: ");
    Serial.print(AudioMemoryUsage());
    Serial.print("/");
    Serial.print(AudioMemoryUsageMax());
    Serial.print(",   ");
    Serial.print("Dyn MEM Float32 Cur/Peak: ");
    Serial.print(AudioMemoryUsage_F32());
    Serial.print("/");
    Serial.print(AudioMemoryUsageMax_F32());
    Serial.println();

    lastUpdate_millis = curTime_millis; //we will use this value the next time around.
  }
}
