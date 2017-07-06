/*
   BasicCompressor_Float

   Created: Chip Audette, Dec 2016 - Jan 2017
   Purpose: Process audio by applying a single-band compressor
            Demonstrates audio processing using floating point data type.

   Uses Teensy Audio Adapter.
   Assumes microphones (or whatever) are attached to the LINE IN (stereo)

   MIT License.  use at your own risk.
*/

//These are the includes from the Teensy Audio Library
#include <Audio.h>   //Teensy Audio Librarya
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include <OpenAudio_ArduinoLibrary.h> //for AudioConvert_I16toF32, AudioConvert_F32toI16, and AudioEffectGain_F32

//create audio library objects for handling the audio
AudioControlSGTL5000_Extended    sgtl5000;    //controller for the Teensy Audio Board
AudioInputI2S             i2s_in;             //Digital audio *from* the Teensy Audio Board ADC.  Sends Int16.  Stereo.
AudioConvert_I16toF32     int2Float1, int2Float2;    //Converts Int16 to Float.  See class in AudioStream_F32.h
AudioEffectCompressor_F32 comp1, comp2;
AudioConvert_F32toI16     float2Int1, float2Int2;    //Converts Float to Int16.  See class in AudioStream_F32.h
AudioOutputI2S            i2s_out;            //Digital audio *to* the Teensy Audio Board DAC.  Expects Int16.  Stereo

//Make all of the audio connections, with the option of USB audio in and out
//note that you ALWAYS have to have an I2S connection (either in or out) to have a clock
//that drives the system.
#define DO_USB  0    //set to 1 to enable USB audio.  Be sure to go under the "Tools" menu and do "USB Type" -> "Audio"
#if DO_USB
  AudioInputUSB           usb_in;
  AudioConnection         patchCord1(usb_in, 0, int2Float1, 0);
  AudioConnection         patchCord2(usb_in, 1, int2Float2, 0);
#else
  AudioConnection         patchCord1(i2s_in, 0, int2Float1, 0);   //connect the Left input to the Left Int->Float converter
  AudioConnection         patchCord2(i2s_in, 1, int2Float2, 0);   //connect the Right input to the Right Int->Float converter
#endif
AudioConnection_F32     patchCord10(int2Float1, 0, comp1, 0); //Left.  makes Float connections between objects
AudioConnection_F32     patchCord11(int2Float2, 0, comp2, 0); //Right.  makes Float connections between objects
AudioConnection_F32     patchCord12(comp1, 0, float2Int1, 0); //Left.  makes Float connections between objects
AudioConnection_F32     patchCord13(comp2, 0, float2Int2, 0); //Right.  makes Float connections between objects
AudioConnection         patchCord20(float2Int1, 0, i2s_out, 0);  //connect the Left float processor to the Left output
AudioConnection         patchCord21(float2Int2, 0, i2s_out, 1);  //connect the Right float processor to the Right output
#if DO_USB
  AudioOutputUSB          usb_out;
  AudioConnection         patchCord30(float2Int1, 0, usb_out, 0);  //connect the Left float processor to the Left output
  AudioConnection         patchCord31(float2Int2, 0, usb_out, 1);  //connect the Right float processor to the Right output
#endif

// which input on the audio shield will be used?
const int myInput = AUDIO_INPUT_LINEIN;
//const int myInput = AUDIO_INPUT_MIC;

//I have a potentiometer on the Teensy Audio Board
#define POT_PIN A1  //potentiometer is tied to this pin

//define a function to setup the Teensy Audio Board how I like it
void setupMyAudioBoard(void) {
  sgtl5000.enable();                   //start the audio board
  sgtl5000.inputSelect(myInput);       //choose line-in or mic-in
  sgtl5000.volume(0.8);                //volume can be 0.0 to 1.0.  0.5 seems to be the usual default.
  sgtl5000.lineInLevel(10, 10);        //level can be 0 to 15.  5 is the Teensy Audio Library's default
  sgtl5000.adcHighPassFilterDisable(); //reduces noise.  https://forum.pjrc.com/threads/27215-24-bit-audio-boards?p=78831&viewfull=1#post78831
  sgtl5000.micBiasEnable(3.0);         //enable the mic bias voltage...only in AudioControlSGTL5000_Extended
}

//define a function to configure the left and right compressors
void setupMyCompressors(boolean use_HP_filter, float knee_dBFS, float comp_ratio, float attack_sec, float release_sec) {
  comp1.enableHPFilter(use_HP_filter);   comp2.enableHPFilter(use_HP_filter);
  comp1.setThresh_dBFS(knee_dBFS);       comp2.setThresh_dBFS(knee_dBFS);
  comp1.setCompressionRatio(comp_ratio); comp2.setCompressionRatio(comp_ratio);

  float fs_Hz = AUDIO_SAMPLE_RATE;
  comp1.setAttack_sec(attack_sec, fs_Hz);       comp2.setAttack_sec(attack_sec, fs_Hz);
  comp1.setRelease_sec(release_sec, fs_Hz);     comp2.setRelease_sec(release_sec, fs_Hz);
}

// define the overall setup() function, the function that is called once when the device is booting
void setup() {
  Serial.begin(115200);   //open the USB serial link to enable debugging messages
  delay(500);             //give the computer's USB serial system a moment to catch up.
  Serial.println("Teensy Hearing Aid: BasicCompressor_Float..."); //identify myself over the USB serial

  // Audio connections require memory, and the record queue uses this memory to buffer incoming audio.
  AudioMemory(14);  //allocate Int16 audio data blocks
  AudioMemory_F32(16); //allocate Float32 audio data blocks

  // Enable the audio shield, select input, and enable output
  setupMyAudioBoard();

  //choose the compressor parameters...note that preGain is set by the potentiometer in the main loop()
  boolean use_HP_filter = true; //enable the software HP filter to get rid of DC?
  float knee_dBFS, comp_ratio, attack_sec, release_sec;
  if (false) {
      Serial.println("Configuring Compressor for fast response for use as a limitter.");
      knee_dBFS = -15.0f; comp_ratio = 5.0f;  attack_sec = 0.005f;  release_sec = 0.200f;
  } else {
      Serial.println("Configuring Compressor for slow response more like an automatic volume control.");
      knee_dBFS = -50.0; comp_ratio = 5.0;  attack_sec = 1.0;  release_sec = 2.0;
  }

  //configure the left and right compressors with the desired settings
  setupMyCompressors(use_HP_filter, knee_dBFS, comp_ratio, attack_sec, release_sec);

  // setup any other other features
  pinMode(POT_PIN, INPUT); //set the potentiometer's input pin as an INPUT

} //end setup()


// define the loop() function, the function that is repeated over and over for the life of the device
unsigned long updatePeriod_millis = 100; //how many milliseconds between updating gain reading?
unsigned long lastUpdate_millis = 0;
unsigned long curTime_millis = 0;
int prev_gain_dB = 0;
unsigned long lastMemUpdate_millis = 0;
void loop() {
  //choose to sleep ("wait for interrupt") instead of spinning our wheels doing nothing but consuming power
  asm(" WFI");  //ARM-specific.  Will wake on next interrupt.  The audio library issues tons of interrupts, so we wake up often.

  //has enough time passed to try updating the GUI?
  curTime_millis = millis(); //what time is it right now
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?

    //read potentiometer
    float32_t val = float(analogRead(POT_PIN)) / 1024.0f; //0.0 to 1.0
    val = 0.1 * (float)((int)(10.0 * val + 0.5)); //quantize so that it doesn't chatter

    //compute desired digital gain
    const float min_gain_dB = -20.0, max_gain_dB = 40.0; //set desired gain range
    float gain_dB = min_gain_dB + (max_gain_dB - min_gain_dB) * val; //computed desired gain value in dB

    //if the gain is different than before, set the new gain value
    if (abs(gain_dB - prev_gain_dB) > 1.0) { //is it different than before
      prev_gain_dB = gain_dB;  //we will use this value the next time around
      
      //gain_dB = 0.0; //force to 0 dB for debugging
      comp1.setPreGain_dB(gain_dB);  //set the gain of the Left-channel gain processor
      comp2.setPreGain_dB(gain_dB);  //set the gain of the Right-channel gain processor
      Serial.print("Setting Digital Pre-Gain dB = "); Serial.println(gain_dB); //print text to Serial port for debugging
    }

    lastUpdate_millis = curTime_millis; //we will use this value the next time around.
  } // end if


  //print status information to the Serial port
  if ((curTime_millis - lastMemUpdate_millis) > 2000) {  // print a summary of the current & maximum usage
    //printCompressorState(&Serial);
    printCPUandMemoryUsage(&Serial);
    lastMemUpdate_millis = curTime_millis; //we will use this value the next time around.
  }

} //end loop();

void printCompressorState(Stream *s) {
  s->print("Current Compressor: Pre-Gain (dB) = ");
  s->print(comp1.getPreGain_dB());
  s->print(", Level (dBFS) = ");
  s->print(comp1.getCurrentLevel_dBFS());
  s->print(", ");
  s->print(comp2.getCurrentLevel_dBFS());
  s->print(", Dynamic Gain L/R (dB) = ");
  s->print(comp1.getCurrentGain_dB());
  s->print(", ");
  s->print(comp2.getCurrentGain_dB());
  s->println();
};

void printCPUandMemoryUsage(Stream *s) {
  s->print("Usage/Max: ");
  s->print("comp1 CPU = "); s->print(comp1.processorUsage()); s->print("/"); s->print(comp1.processorUsageMax()); s->print(", ");
  s->print("all CPU = " ); s->print(AudioProcessorUsage()); s->print("/");  s->print(AudioProcessorUsageMax()); s->print(", ");
  s->print("Int16 Mem = "); s->print(AudioMemoryUsage()); s->print("/"); s->print(AudioMemoryUsageMax()); s->print(", ");
  s->print("Float Mem = "); s->print(AudioMemoryUsage_F32()); s->print("/"); s->print(AudioMemoryUsageMax_F32()); s->print(", ");
  s->println();
};

