/* FineFreqShift_OA.ino
 * 
 * 1 Hz Resolution Frequency Shift and Synthetic Stereo
 * 
 * Written for Open Audio Bob Larkin June 2020
 *
 * This sketch demonstrates the use of dual multipliers along with broad-band 90-degree
 * phase shifters (Hilbert transforms)  and sine/cosine generator to shift all frequencies
 * by a fixed amount.  The shift amount is set by a digital oscillator frequency
 * and can be made arbitrarily precise. Thus the name "Fine Frequency Shifter."
 *
 * Two blocks do most of the work of shifting the frequency band. The RadioIQMixer_F32
 * has a pair of multipliers that have one of their inputs from an oscillator that produces
 * a sine and a cosine waveform of the same frequency.  This oscillator phase difference
 * is transferred to the output producing phase differences of 90-degrees.  The multipliers,
 * called double-balanced mixers in the analog world, produce sum and difference frequencies
 * but the original input frequencies are suppressed.  In the analog case, this suppression
 * is limited by circuit balance, but for digital multipliers, the suppression is
 * close-to-perfect.  Thus the outputs of the I-Q Mixers is a pair of frequency  sum and
 * difference signals.  Since the oscillator can be a low frequency, such as 100 Hz,
 * the frequency band of sum and difference signals will include much overlap.
 *
 * The next block, the "AudioFilter90Deg_F32" applies a constant phase difference between
 * the two inputs, which are then added or subtracted. That turns out to cause either the
 * frequency sum or difference to be cancelled out.  At that point only one audio signal
 * remains and it is frequency shifted by the oscillator frequency.
 * 
 * This INO also demonstrates the conversion of monaural to stereo sound by
 * delaying one channel.  This technique has been used for many years to de-correlate
 * monaural noise before sending it to both ears.  This can engage the the human brain
 * to hear correlated signals better.  It also adds a stereo presence to monaural voice
 * that is pleasant to hear.  It is tossed in here for experimentation.  In general,
 * delays up to 20 msec give the illusion of presence, whereas larger delays start to
 * sound like loud echos!  Play with it.  Headphones vs. speakers change the perception.
 * 
 * Control is done over the "USB Serial" using the Serial Monitor of the Arduino
 * IDE.  The commands control most functions and a list can be seen by typing
 * "h<enter>", or looking in the listing below, at printHelp().
 * 
 * Refs - The phasing method of SSB generation goes way back.
 *    https://en.wikipedia.org/wiki/Single-sideband_modulation#Hartley_modulator
 * The precision of DSP makes it practical for overlapping input and output bands.
 * I first encountered this in conversation with friend Johan Forrer: 
 *    https://www.arrl.org/files/file/Technology/tis/info/pdf/9609x008.pdf
 * I need to find the German description of delay stereo from the 1980's.  Both audio
 * shifting and delay stereo were put into the DSP-10 audio processor:
 *    http://www.janbob.com/electron/dsp10/dsp10.htm
 * 
 * Tested OK for Teensy 3.6 and 4.0.
 * For settings:
 *   sample rate (Hz) = 44117.00
 *   block size (samples) = 128
 *   N_FFT = 512
 *   Hilbert 251 taps
 * CPU Cur/Peak, Teensy 3.6: 27.28%/27.49%
 * CPU Cur/Peak, Teensy 4.0: 5.82%/5.84%
 * Memory useage is 4 for I16 Memory
 * Memory for F32 is 6 plus 1 more for every 2.9 mSec of Stereo Delay.
 *
 * This INO sketch is in the public domain.
 */

#include "Audio.h"
#include "AudioStream_F32.h"
#include "OpenAudio_ArduinoLibrary.h"

//set the sample rate and block size
const float sample_rate_Hz = 44117.f;
const int audio_block_samples = 128;
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

//create audio library objects for handling the audio
AudioInputI2S            i2sIn;     // This I16 input/output is T4.x compatible
AudioConvert_I16toF32    cnvrt1;    // Convert to float
RadioIQMixer_F32         iqmixer1;
AudioFilter90Deg_F32     hilbert1;
AudioMixer4_F32          sum1;      // Summing node for the SSB receiver
AudioFilterFIR_F32       fir1;      // Low Pass Filter to frequency limit the SSB
AudioEffectDelay_OA_F32  delay1;    // Pleasant and useful sound between L & R
AudioConvert_F32toI16    cnvrt2;
AudioConvert_F32toI16    cnvrt3;
AudioOutputI2S           i2sOut;
AudioControlSGTL5000     codec;
//Make all of the audio connections
AudioConnection       patchCord0(i2sIn,    0, cnvrt1,   0); // connect to Left codec, 16-bit
AudioConnection_F32   patchCord1(cnvrt1,   0, iqmixer1, 0); // Input to 2 mixers
AudioConnection_F32   patchCord2(cnvrt1,   0, iqmixer1, 1);
AudioConnection_F32   patchCord3(iqmixer1, 0, hilbert1, 0); // Broadband 90 deg phase
AudioConnection_F32   patchCord4(iqmixer1, 1, hilbert1, 1);
AudioConnection_F32   patchCord5(hilbert1, 0, sum1,     0); // Sideband select
AudioConnection_F32   patchCord6(hilbert1, 1, sum1,     1);
AudioConnection_F32   patchCord7(sum1,     0, delay1,   0); // delay channel 0
AudioConnection_F32   patchCord9(sum1,     0, cnvrt2,   0); // connect to the left output
AudioConnection_F32   patchCordA(delay1,   0, cnvrt3,   0); // right output
AudioConnection       patchCordB(cnvrt2,   0, i2sOut,   0);
AudioConnection       patchCordC(cnvrt3,   0, i2sOut,   1);

//control display and serial interaction
bool enable_printCPUandMemory = false;
void togglePrintMemoryAndCPU(void) { enable_printCPUandMemory = !enable_printCPUandMemory; };

// Filter for AudioFilter90Deg_F32 hilbert1
#include "hilbert251A.h"

//inputs and levels
float gain_dB = -15.0f;
float gain = 0.177828f;  // Same as -15 dB
float sign = 1.0f;
float deltaGain_dB = 2.5f;
float frequencyLO = 100.0f;
float delayms = 1.0f;

// ***************   SETUP   **********************************
void setup() {
  Serial.begin(1); delay(1000);
  Serial.println("*** Fine Frequency Shifter - June 2020 ***");
  Serial.print("Sample Rate in Hz = ");
  Serial.println(audio_settings.sample_rate_Hz, 0);
  Serial.print("Block size, samples = ");
  Serial.println(audio_settings.audio_block_samples);

  AudioMemory(10);            // I16 type
  AudioMemory_F32(200, audio_settings);

  //Enable the codec to start the audio flowing!
  codec.enable();
  codec.adcHighPassFilterEnable();
  codec.inputSelect(AUDIO_INPUT_LINEIN);

  iqmixer1.frequency(frequencyLO);   // Frequency shift, Hz
  deltaFrequency(0.0f);              // Print freq
  hilbert1.begin(hilbert251A, 251);  // Set the Hilbert transform FIR filter
  sum1.gain(0, gain*sign);           // Set gains
  sum1.gain(1, gain);
  delay1.delay(0, delayms);          // Delay right channel
  deltaDelay(0.0f);                  // Print delay
  
  //finish the setup by printing the help menu to the serial connections
  printHelp();
}

// *************************   LOOP    ****************************
void loop() {
  //respond to Serial commands
  while (Serial.available()) respondToByte((char)Serial.read());

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
    Serial.println("%");
    Serial.print(" Audio MEM Float32 Cur/Peak: ");
    Serial.print(AudioMemoryUsage_F32());
    Serial.print("/");
    Serial.println(AudioMemoryUsageMax_F32());
    Serial.print(" Audio MEM Int16 Cur/Peak: ");
    Serial.print(AudioMemoryUsage());
    Serial.print("/");
    Serial.println(AudioMemoryUsageMax());

    lastUpdate_millis = curTime_millis; //we will use this value the next time around.
  }
}

void incrementGain(float increment_dB) {
  gain_dB += increment_dB;
  // gain is set in the "mixer" block, including sign for raise/lower freq
  gain = powf(10.0f, 0.05f * gain_dB);
  sum1.gain(0, gain*sign);
  sum1.gain(1, gain);
  printGainSettings();
}

void printGainSettings(void) {
  Serial.print("Gain in dB = ");    Serial.println(gain_dB, 1);
}

void deltaFrequency(float dfr) {
    frequencyLO += dfr;
    Serial.print("Frequency shift in Hz = ");
    Serial.println(frequencyLO, 1);
    if(frequencyLO < 0.0f)
        sign = 1.0f;
    else
        sign = -1.0f;
    iqmixer1.frequency(fabsf(frequencyLO));
    incrementGain(0.0f);
}

void deltaDelay(float dtau) {
    delayms += dtau;
    if (delayms < 0.0f)
       delayms = 0.0f;
    delay1.delay(0, delayms);   // Delay right channel
    Serial.print("Delay in milliseconds = ");
    Serial.println(delayms, 1);
}

//switch yard to determine the desired action
void respondToByte(char c) {
  char s[2];
  s[0] = c;  s[1] = 0;
  if( !isalpha((int)c) && c!='?') return;
  switch (c) {
    case 'h': case '?':
      printHelp();
      break;
    case 'g': case 'G':
      printGainSettings();
      break;
    case 'k':
      incrementGain(deltaGain_dB);
      break;
    case 'K':   // which is "shift k"
      incrementGain(-deltaGain_dB);  
      break;
    case 'C': case 'c':
      Serial.println("Toggle printing of memory and CPU usage.");
      togglePrintMemoryAndCPU();
      break;
    case 'd':
      deltaFrequency(1.0f);
      break;
    case 'D':
      deltaFrequency(-1.0f);
      break;
    case 'e':
      deltaFrequency(10.0f);
      break;
    case 'E':
      deltaFrequency(-10.0f);
      break;
    case 'f':
      deltaFrequency(100.0f);
      break;
    case 'F':
      deltaFrequency(-100.0f);
      break;
    case 't':
      deltaDelay(1.0f);
      break;
    case 'T':
      deltaDelay(-1.0f);
      break;
    case 'u':
      deltaDelay(10.0f);
      break;
    case 'U':
      deltaDelay(-10.0f);
      break;      
    default:
      Serial.print("You typed "); Serial.print(s);
      Serial.println(".  What command?");
  }
}

void printHelp(void) {
  Serial.println();
  Serial.println("Help: Available Commands:");
  Serial.println("   h: Print this help");
  Serial.println("   g: Print the gain settings of the device.");
  Serial.println("   C: Toggle printing of CPU and Memory usage");
  Serial.print(  "   k: Increase the gain of both channels by ");
         Serial.print(deltaGain_dB); Serial.println(" dB");
  Serial.print(  "   K: Decrease the gain of both channels by ");
         Serial.print(-deltaGain_dB); Serial.println(" dB");
  Serial.println("   d: Raise frequency by 1 Hz");
  Serial.println("   D: Lower frequency by 1 Hz");
  Serial.println("   e: Raise frequency by 10 Hz");
  Serial.println("   E: Lower frequency by 10 Hz");
  Serial.println("   f: Raise frequency by 100 Hz");
  Serial.println("   F: Lower frequency by 100 Hz");
  Serial.println("   t: Raise stereo delay by 1 msec");
  Serial.println("   T: Lower stereo delay by 1 msec");
  Serial.println("   u: Raise stereo delay by 10 msec");
  Serial.println("   U: Lower stereo delay by 10 msec");
}
