/* FormantShifter_FD.ino
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
 * The amount of formant shifting is controled via the Serial link.
 *    It defaults to a modest upward shifting of the formants
 *
 * Built for the Tympan library for Teensy 3.6-based hardware
 *
 * Adapt to OpenAudio Library  - Bob Larkin June 2020
 * Ref: http://iris.elf.stuba.sk/JEEEC/data/pdf/1_110-08.pdf
 *
 * MIT License.  Use at your own risk.
 *
 */
#include "AudioStream_F32.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioEffectFormantShiftFD_OA_F32.h"  //the local file holding your custom function
#include "SerialManager_OA.h"

//set the sample rate and block size
const float sample_rate_Hz = 44117.f; ; //24000 or 44117 (or other frequencies in the table in AudioOutputI2S_F32)
const int audio_block_samples = 128;     //for freq domain processing choose a power of 2 (16, 32, 64, 128) but no higher than 128
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

//create audio library objects for handling the audio
AudioInputI2S                 i2sIn;
AudioConvert_I16toF32         cnvrt1;
AudioEffectFormantShiftFD_F32 formantShift(audio_settings); //create the frequency-domain processing block
AudioEffectGain_F32           gain1; //Applies digital gain to audio data.
AudioConvert_F32toI16         cnvrt2;
AudioOutputI2S                i2sOut;
AudioControlSGTL5000          codec;
//Make all of the audio connections
AudioConnection       patchCord1(i2sIn,        0, cnvrt1, 0); // connect to Left codec, 16-bit
AudioConnection_F32   patchCord2(cnvrt1,       0, formantShift, 0);
AudioConnection_F32   patchCord2(formantShift, 0, gain1, 0); //connect to gain
AudioConnection_F32   patchCord3(gain1,        0, cnvrt2, 0); //connect to the left output
AudioConnection       patchCord6(cnvrt2,       0, i2sOut, 0);

//control display and serial interaction
bool enable_printCPUandMemory = false;
void togglePrintMemoryAndCPU(void) { enable_printCPUandMemory = !enable_printCPUandMemory; };
SerialManager_OA SerialManager_OA(audioHardware);

//inputs and levels
float input_gain_dB = 20.0f; //gain on the microphone
float formant_shift_gain_correction_dB = 0.0;  //will be used to adjust for gain in formant shifter

// define the setup() function, the function that is called once when the device is booting
void setup() {
  Serial.begin(1); delay(1000);
  mySerial.println("FormantShifter: starting setup()...");
  mySerial.print("    : sample rate (Hz) = "); mySerial.println(audio_settings.sample_rate_Hz);
  mySerial.print("    : block size (samples) = "); mySerial.println(audio_settings.audio_block_samples);

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(3);            // I16 type
  AudioMemory_F32(40, audio_settings);

  // Configure the frequency-domain algorithm
  int overlap_factor = 4;  //set to 4 or 8 or either 75% overlap (4x) or 87.5% overlap (8x)
  int N_FFT = audio_block_samples * overlap_factor;
  formantShift.setup(audio_settings, N_FFT); //do after AudioMemory_F32();
  formantShift.setScaleFactor(1.5); //1.0 is no formant shifting.
  if (overlap_factor == 4) {
    formant_shift_gain_correction_dB = -3.0;
  } else if (overlap_factor == 8) {
    formant_shift_gain_correction_dB = -9.0;
  }

  codec.enable(); // activate AIC

  //setup DC-blocking highpass filter running in the ADC hardware itself

  //Choose the desired input


  //Set the desired volume levels
  audioHardware.volume_dB(0);                   // headphone amplifier.  -63.6 to +24 dB in 0.5dB steps.

  //finish the setup by printing the help menu to the serial connections
  Serial.printHelp();
}


// define the loop() function, the function that is repeated over and over for the life of the device
void loop() {

  //respond to Serial commands
  while (Serial.available()) SerialManager_OA.respondToByte((char)Serial.read());   //USB Serial
  //while (Serial1.available()) SerialManager_OA.respondToByte((char)Serial1.read()); //BT Serial

  //check the potentiometer
  servicePotentiometer(millis(), 100); //service the potentiometer every 100 msec

  //check to see whether to print the CPU and Memory Usage
  if (enable_printCPUandMemory) printCPUandMemory(millis(), 3000); //print every 3000 msec

} //end loop();


// ///////////////// Servicing routines

//servicePotentiometer: listens to the blue potentiometer and sends the new pot value
//  to the audio processing algorithm as a control parameter
void servicePotentiometer(unsigned long curTime_millis, const unsigned long updatePeriod_millis) {
  //static unsigned long updatePeriod_millis = 100; //how many milliseconds between updating the potentiometer reading?
  static unsigned long lastUpdate_millis = 0;
  static float prev_val = -1.0;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?

    //read potentiometer
    float val = float(audioHardware.readPotentiometer()) / 1023.0; //0.0 to 1.0
    val = (1.0/9.0) * (float)((int)(9.0 * val + 0.5)); //quantize so that it doesn't chatter...0 to 1.0

      #if 0
        //set the volume of the system
        setVolKnobGain_dB(val*45.0f - 10.0f - input_gain_dB);
      #else
        //set the amount of formant shifting
        float new_scale_fac = powf(2.0,(val-0.5)*2.0);
        formantShift.setScaleFactor(new_scale_fac);
      #endif
    }


    lastUpdate_millis = curTime_millis;
  } // end if
} //end servicePotentiometer();



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
    Serial.print("Dyn MEM Float32 Cur/Peak: ");
    Serial.print(AudioMemoryUsage_F32());
    Serial.print("/");
    Serial.print(AudioMemoryUsageMax_F32());
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


void incrementKnobGain(float increment_dB) { //"extern" to make it available to other files, such as SerialManager_OA.h
  setVolKnobGain_dB(vol_knob_gain_dB+increment_dB);
}

void setVolKnobGain_dB(float gain_dB) {
  vol_knob_gain_dB = gain_dB;
gain1.setGain_dB(vol_knob_gain_dB+formant_shift_gain_correction_dB);
  printGainSettings();
}

float incrementFormantShift(float incr_factor) {
  float cur_scale_factor = formantShift.getScaleFactor();
  return formantShift.setScaleFactor(cur_scale_factor*incr_factor);
}


