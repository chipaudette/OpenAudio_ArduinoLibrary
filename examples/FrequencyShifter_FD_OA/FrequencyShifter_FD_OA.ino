/* FreqShifter_FD_OA.ino
 *
 * Demonstrate frequency shifting via frequency domain processin.
 *
 * Created: Chip Audette (OpenAudio) Aug 2019
 *
 * Approach: This processing is performed in the frequency domain.
 *    Frequencies can only be shifted by an integer number of bins,
 *    so small frequency shifts are not possible.  For example, for
 *    a sample rate of 44.1kHz, and when using N=256, one can only
 *    shift frequencies in multiples of 44.1/256 = 172.3 Hz.
 *
 *    This processing is performed in the frequency domain where
 *    we take the FFT, shift the bins upward or downward, take
 *    the IFFT, and listen to the results.  In effect, this is
 *    single sideband modulation, which will sound very unnatural
 *    (like robot voices!).  Maybe you'll like it, or maybe not.
 *    Probably not, unless you like weird.  ;)
 *
 *    You can shift frequencies upward or downward with this algorithm.
 *
 * Frequency Domain Processing:
 *    * Take samples in the time domain
 *    * Take FFT to convert to frequency domain
 *    * Manipulate the frequency bins to do the freqyebct shifting
 *    * Take IFFT to convert back to time domain
 *    * Send samples back to the audio interface
 *
 * Built for the Tympan library for Teensy 3.6-based hardware
 *
 * Convert to Open Audio Bob Larkin June 2020
 * Tested OK for Teensy 3.6 and 4.0.
 * For settings:
 *   sample rate (Hz) = 44117.00
 *   block size (samples) = 128
 *   N_FFT = 512
 * the following resources were used for Teensy 3.6
 *   CPU Cur/Peak: 50.02%/50.24%
 *   MEM Float32 Cur/Peak: 8/9
 *   MEM Int16 Cur/Peak:   3/5
 * For Teensy 4.0:
 *   CPU Cur/Peak: 6.53%/6.84%
 *   MEM Float32 Cur/Peak: 8/9
 *   MEM Int16 Cur/Peak:   3/4
 *
 * MIT License.  Use at your own risk.
 */

#include "Audio.h"
#include "AudioStream_F32.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "SerialManager_FreqShift_OA.h"

//set the sample rate and block size
const float sample_rate_Hz = 44117.f; ; // other frequencies in the table in AudioOutputI2S_F32 for T3.x only
const int audio_block_samples = 128;     //for freq domain processing, a power of 2: 16, 32, 64, 128
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

//create audio library objects for handling the audio
AudioInputI2S                 i2sIn;   // This I16 input/output is T4.x compatible
AudioConvert_I16toF32         cnvrt1;  // Convert to float
AudioEffectFreqShiftFD_OA_F32 freqShift(audio_settings); // the frequency-domain processing block
AudioEffectGain_F32           gain1;   //Applies digital gain to audio data.
AudioConvert_F32toI16         cnvrt2;
AudioOutputI2S                i2sOut;
AudioControlSGTL5000          codec;
//Make all of the audio connections
AudioConnection       patchCord1(i2sIn,        0, cnvrt1, 0); // connect to Left codec, 16-bit
AudioConnection_F32   patchCord2(cnvrt1,       0, freqShift, 0);
AudioConnection_F32   patchCord3(freqShift, 0, gain1, 0); //connect to gain
AudioConnection_F32   patchCord4(gain1,        0, cnvrt2, 0); //connect to the left output
AudioConnection       patchCord6(cnvrt2,       0, i2sOut, 0);

//control display and serial interaction
bool enable_printCPUandMemory = false;
void togglePrintMemoryAndCPU(void) { enable_printCPUandMemory = !enable_printCPUandMemory; };
SerialManagerFreqShift_OA serialMgr;

//inputs and levels
float input_gain_dB = 15.0f; //gain on the microphone
float vol_knob_gain_dB = 0.0;      //will be overridden by volume knob
      
// ***************   SETUP   **********************************
void setup() {
  Serial.begin(1); delay(1000);
  Serial.println("freqShifter: starting setup()...");
  Serial.print("    : sample rate (Hz) = ");  Serial.println(audio_settings.sample_rate_Hz);
  Serial.print("    : block size (samples) = ");  Serial.println(audio_settings.audio_block_samples);

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(10);            // I16 type
  AudioMemory_F32(40, audio_settings);

  // Configure the FFT parameters algorithm
  int overlap_factor = 4;  //set to 2, 4 or 8...which yields 50%, 75%, or 87.5% overlap (8x)
  int N_FFT = audio_block_samples * overlap_factor;  
  Serial.print("    : N_FFT = "); Serial.println(N_FFT);
  freqShift.setup(audio_settings, N_FFT); //do after AudioMemory_F32();

  //configure the frequency shifting
  float shiftFreq_Hz = 750.0; //shift audio upward a bit
  float Hz_per_bin = audio_settings.sample_rate_Hz / ((float)N_FFT);
  int shift_bins = (int)(shiftFreq_Hz / Hz_per_bin + 0.5);  //round to nearest bin
  
  shiftFreq_Hz = shift_bins * Hz_per_bin;
  Serial.print("Setting shift to "); Serial.print(shiftFreq_Hz);
     Serial.print(" Hz, which is "); Serial.print(shift_bins); 
     Serial.println(" bins");
  freqShift.setShift_bins(shift_bins); //0 is no ffreq shifting.
 
  //Enable the Tympan to start the audio flowing!
  codec.enable();
  codec.adcHighPassFilterEnable();
  codec.inputSelect(AUDIO_INPUT_LINEIN);

  //finish the setup by printing the help menu to the serial connections
  serialMgr.printHelp();
}

// *************************   LOOP    ****************************
void loop() {

  //respond to Serial commands
  while (Serial.available()) serialMgr.respondToByte((char)Serial.read());   //USB Serial
 
  //check to see whether to print the CPU and Memory Usage
  if (enable_printCPUandMemory) printCPUandMemory(millis(), 3000); //print every 3000 msec

} //end loop();

//This routine prints the current and maximum CPU usage and the current usage of the AudioMemory that has been allocated
void printCPUandMemory(unsigned long curTime_millis, unsigned long updatePeriod_millis) {
  //static unsigned long updatePeriod_millis = 3000; //how many milliseconds between updating gain reading?
  static unsigned long lastUpdate_millis = 0;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?
    Serial.print("CPU Cur/Peak: ");
    Serial.print(audio_settings.processorUsage());
    Serial.print("%/");
    Serial.print(audio_settings.processorUsageMax());
    Serial.print("%,   ");
    Serial.print(" Dyn MEM Float32 Cur/Peak: ");
    Serial.print(AudioMemoryUsage_F32());
    Serial.print("/");
    Serial.print(AudioMemoryUsageMax_F32());
    Serial.print(" MEM Int16 Cur/Peak: ");
    Serial.print(AudioMemoryUsage());
    Serial.print("/");
    Serial.print(AudioMemoryUsageMax());
    Serial.println();

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
  setVolKnobGain_dB(vol_knob_gain_dB+increment_dB);
}

void setVolKnobGain_dB(float gain_dB) {
  vol_knob_gain_dB = gain_dB;
  gain1.setGain_dB(vol_knob_gain_dB);
  printGainSettings();
}

int incrementFreqShift(int incr_factor) {
  int cur_shift_bins = freqShift.getShift_bins();
  return freqShift.setShift_bins(cur_shift_bins + incr_factor);
}
