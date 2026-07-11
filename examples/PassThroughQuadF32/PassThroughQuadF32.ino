// Demonstration of AudioOutputI2SQuad_F32 four channel I2S output object.
// Greg Raven KF5N November 2025.
// The first left and right channels are output on pin 7, which drives the
// Teensy Audio Adapter (Teensy 4.1).  The second left and right channels are output on pin 32 (Teensy 4.1).

#include <Arduino.h>
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>
#include <AudioStream_F32.h>

const int sample_rate_Hz = 48000;
const int audio_block_samples = 128;   // Always 128

AudioControlSGTL5000 sgtl5000_1;
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

AudioAnalyzePeak_F32        peak0L(audio_settings);
AudioAnalyzePeak_F32        peak1R(audio_settings);
AudioAnalyzePeak_F32        peak2L(audio_settings);
AudioAnalyzePeak_F32        peak3R(audio_settings);
AudioInputI2SQuad_F32 i2s_in(audio_settings);
AudioOutputI2SQuad_F32 i2s_out(audio_settings);

AudioConnection_F32          connect0(i2s_in,   0, peak0L,  0);
AudioConnection_F32          connect1(i2s_in,   1, peak1R,  0);
AudioConnection_F32          connect2(i2s_in,   2, peak2L,  0);
AudioConnection_F32          connect3(i2s_in,   3, peak3R,  0);

AudioConnection_F32          patchCord1(i2s_in, 0, i2s_out, 0);
AudioConnection_F32          patchCord2(i2s_in, 1, i2s_out, 1);
AudioConnection_F32          patchCord3(i2s_in, 2, i2s_out, 2);
AudioConnection_F32          patchCord4(i2s_in, 3, i2s_out, 3);
// Select Which Input from Teensy Audio Adaptor	
//const int myInput = AUDIO_INPUT_LINEIN;
const int myInput = AUDIO_INPUT_MIC;

void setup() {

  Serial.begin(115200);

  /* check for CrashReport stored from previous run */
  if (CrashReport) {
    Serial.print(CrashReport);
  }

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory_F32(20,audio_settings);

  // Enable the first audio shield, select input, and enable output
  sgtl5000_1.setAddress(LOW);
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(myInput);
  sgtl5000_1.volume(0.8);  // Set headphone volume.
  sgtl5000_1.unmuteHeadphone();
#if 0
  // Enable the second audio shield, select input, and enable output
  sgtl5000_2.setAddress(HIGH);
  sgtl5000_2.enable();
  sgtl5000_2.inputSelect(myInput);
  sgtl5000_2.volume(0.8);
  sgtl5000_2.unmuteHeadphone();
 #else
	 // Specific code for T41-EP ADC PCM1808/ DAC PCM5102
#define UNMUTEAUDIO LOW
#define MUTEAUDIO   HIGH  
  const int MUTE = 38;          // Mute Audio, HIGH = "On" Audio PA, LOW = Mute Audio PA off.  This may be reversed depending on PA.

  pinMode(MUTE, OUTPUT);
  Serial.printf("UNMUTE\n");
  digitalWrite(MUTE, UNMUTEAUDIO);  // Keep audio junk out of the speakers/headphones until configuration is complete.

 #endif

}

void loop() {
 
  Serial.printf("PassThrough Running Now\n");

  if (myInput == AUDIO_INPUT_MIC){
    Serial.printf("Now using mic!\n");
  } else {
    Serial.printf("Now using line in!\n");
  }	  
  Serial.print("Max float memory = ");
  Serial.println(AudioStream_F32::f32_memory_used_max);
  if(peak0L.available())  Serial.print(peak0L.read(), 6);
  Serial.print(" <-0L   1R-> ");
  if(peak1R.available())  Serial.println(peak1R.read(), 6);
  if(peak2L.available())  Serial.print(peak2L.read(), 6);
  Serial.print(" <-2L   3R-> ");
  if(peak3R.available())  Serial.println(peak3R.read(), 6);
  delay(1000);
// DMA debug code.
  uint32_t* dmaErrors;
  dmaErrors = (uint32_t*)(0x400E8000 + 0x4);
  Serial.printf("DMA errors = %x\n", *dmaErrors);
//

}
