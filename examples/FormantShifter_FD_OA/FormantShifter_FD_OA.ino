/* FormantShifter_FD_OA.ino
 *
 * Demonstrate formant shifting via frequency domain processin.
 *
 * Created: Chip Audette (OpenAudio) March 2019
 *
 * Approach:
 *    * Take samples in the time domain
 *    * Take FFT to convert to frequency domain
 *    * Manipulate the frequency bins to do the formant shifting
 *    * Take IFFT to convert back to time domain
 *    * Send samples back to the audio interface
 *
 * The amount of formant shifting is controLled via the Serial link.
 * It defaults to a modest upward shifting of the formants
 *
 * Built for the Tympan library for Teensy 3.6-based hardware
 *
 * Adapt to OpenAudio Library  - Bob Larkin June 2020
 * This example supports only audio Line In, left channel, of SGTL5000
 * Codec (Teensy Audio Adaptor) and Line Out on left and right channel.
 * Simplified control by using only serial commands.
 * 
 * This is an interesting block. Try it!  Ask the internet about 
 * formant shifting, and check out Chip's implementation in update()
 * of AudioEffectFormantShiftFD_OA_F32.h
 * 
 * This is tested to run on Teensy 3.6 and Teensy 4.0 using the PJRC
 * Teensy Audio Adaptor.
 * 
 * MIT License.  Use at your own risk.
 */
#include "AudioStream_F32.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioEffectFormantShiftFD_OA_F32.h"  //the local file holding your custom function
#include "SerialManagerFormant_OA.h"

//set the sample rate and block size
const float sample_rate_Hz = 44117.f; ; // other frequencies in the table in AudioOutputI2S_F32 for T3.x only
const int audio_block_samples = 128;     //for freq domain processing, a power of 2: 16, 32, 64, 128
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

//create audio library objects for handling the audio
AudioInputI2S                 i2sIn;   // This I16 input/output is T4.x compatible
AudioConvert_I16toF32         cnvrt1;  // Cobevrt to float
AudioEffectFormantShiftFD_OA_F32 formantShift(audio_settings); // the frequency-domain processing block
AudioEffectGain_F32           gain1; //Applies digital gain to audio data.
AudioConvert_F32toI16         cnvrt2;
AudioOutputI2S                i2sOut;
AudioControlSGTL5000          codec;

AudioAnalyzePeak_F32  peak1;
//Make all of the audio connections
AudioConnection       patchCord1(i2sIn,        0, cnvrt1, 0); // connect to Left codec, 16-bit
AudioConnection_F32   patchCord2(cnvrt1,       0, formantShift, 0);
AudioConnection_F32   patchCord3(formantShift, 0, gain1, 0); //connect to gain
AudioConnection_F32   patchCord4(gain1,        0, cnvrt2, 0); //connect to the left output
AudioConnection       patchCord6(cnvrt2,       0, i2sOut, 0);
AudioConnection       patchCord7(cnvrt2,       0, i2sOut, 1);
AudioConnection_F32  connx(gain1, 0, peak1, 0);
//control display and serial interaction
bool enable_printCPUandMemory = false;
void togglePrintMemoryAndCPU(void) { enable_printCPUandMemory = !enable_printCPUandMemory; };
SerialManagerFormant_OA SerMgr;

//inputs and levels
float input_gain_dB = 0.0f; //gain on the microphone
float formant_shift_gain_correction_dB = 0.0;  //will be used to adjust for gain in formant shifter
float vol_knob_gain_dB = 0.0;      //will be overridden by volume knob

// ****************************  SETUP  **********************
void setup() {
  Serial.begin(1); delay(1000);
  Serial.println("FormantShifter: starting setup()...");
  Serial.print("    : sample rate (Hz) = "); Serial.println(audio_settings.sample_rate_Hz);
  Serial.print("    : block size (samples) = "); Serial.println(audio_settings.audio_block_samples);

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(10);            // I16 type
  AudioMemory_F32(40, audio_settings);

  codec.enable();
  codec.adcHighPassFilterDisable();
  codec.inputSelect(AUDIO_INPUT_LINEIN);
  
  // Configure the frequency-domain algorithm
  int overlap_factor = 4;  //set to 4 or 8 or either 75% overlap (4x) or 87.5% overlap (8x)
  int N_FFT = audio_block_samples * overlap_factor;
  formantShift.setup(audio_settings, N_FFT); //do after AudioMemory_F32();
  formantShift.setScaleFactor(1.587401f); //1.0 is no formant shifting.
  if (overlap_factor == 4) {
    formant_shift_gain_correction_dB = -3.0;
  } else if (overlap_factor == 8) {
    formant_shift_gain_correction_dB = -9.0;
  }
  SerMgr.printHelp();
}

// *************************  LOOP  *********************
void loop() {
  //respond to Serial commands
  while (Serial.available()) SerMgr.respondToByte((char)Serial.read());   //USB Serial
 
  //check to see whether to print the CPU and Memory Usage
  if (enable_printCPUandMemory) printCPUandMemory(millis(), 3000); //print every 3000 msec
} 

// ********************************************************

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
    Serial.print("Audio MEM Float32 Cur/Peak: ");
    Serial.print(AudioMemoryUsage_F32());
    Serial.print("/");
    Serial.println(AudioMemoryUsageMax_F32());
    Serial.print("Audio MEM Int16 Cur/Peak: ");
    Serial.print(AudioMemoryUsage());
    Serial.print("/");
    Serial.println(AudioMemoryUsageMax());
    Serial.println();
//if(peak1.available())  Serial.println(peak1.read(), 6);
    lastUpdate_millis = curTime_millis; //we will use this value the next time around.
  }
}

void printGainSettings(void) {
  Serial.print("Gain (dB): ");
  Serial.print("Vol Knob = "); Serial.print(vol_knob_gain_dB,1);
  //Serial.print(", Input PGA = "); Serial.print(input_gain_dB,1);
  Serial.println();
}

void incrementKnobGain(float increment_dB) { 
  setVolKnobGain_dB(vol_knob_gain_dB + increment_dB);
}

void setVolKnobGain_dB(float gain_dB) {
  vol_knob_gain_dB = gain_dB;
  gain1.setGain_dB(vol_knob_gain_dB + formant_shift_gain_correction_dB);
  printGainSettings();
}

float incrementFormantShift(float incr_factor) {
  float cur_scale_factor = formantShift.getScaleFactor();
  return formantShift.setScaleFactor(cur_scale_factor*incr_factor);
}


