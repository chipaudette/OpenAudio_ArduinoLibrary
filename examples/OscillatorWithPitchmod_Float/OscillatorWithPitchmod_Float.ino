/*
   Oscillator with Pitch modulation

   Created: Patrick Radius, Jan 2017
   Purpose: Demonstrates the ability of the oscillator and pitch modulation.
            Demonstrates audio processing using floating point data type.

   Uses Teensy Audio Adapter.

   MIT License.  use at your own risk.
*/

//These are the includes from the Teensy Audio Library
#include <Audio.h>   //Teensy Audio Librarya
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include <OpenAudio_ArduinoLibrary.h> //for AudioConvert_I16toF32, AudioConvert_F32toI16, and AudioEffectGain_F32

#define DO_USB  0    //set to 1 to enable USB audio.  Be sure to go under the "Tools" menu and do "USB Type" -> "Audio"

//create audio library objects for handling the audio
AudioControlSGTL5000_Extended    sgtl5000;     //controller for the Teensy Audio Board
AudioSynthWaveform_F32           osc1;         // Audio-rate oscillator.
AudioSynthWaveform_F32           lfo1;         // Low-frequency oscillator to modulate the main oscillator with.
AudioConvert_F32toI16            float2Int;   //Converts Float to Int16.  See class in AudioStream_F32.h
AudioOutputI2S                   i2s_out;      //Digital audio *to* the Teensy Audio Board DAC.  Expects Int16.  Stereo


AudioConnection_F32     patchCord1(osc1, 0, float2Int, 0);   // connect the oscillator to the float 2 int converter
AudioConnection_F32     patchCord2(lfo1, 0, osc1, 0);         // connect the output of the lfo to the mod input of the oscillator

AudioConnection         patchCord3(float2Int, 0, i2s_out, 0); //Left out.
AudioConnection         patchCord4(float2Int, 0, i2s_out, 1); //Right out.

//For output, you always need an i2s or else you have no clock to drive the audio subsystem.
//So, even if you're not going to use the headphone/line-out, you need these lines 
AudioConnection         patchCord5(float2Int, 0, i2s_out, 0);  //connect the Left float processor to the Left output.
AudioConnection         patchCord6(float2Int, 0, i2s_out, 1);  //connect the Right float processor to the Right output.

//Now, if you want USB output, it gets enabled here
#if DO_USB
  AudioOutputUSB        usb_out;
  AudioConnection       patchCord7(float2Int, 0, usb_out, 0);  //connect the float processor to the Left output
  AudioConnection       patchCord8(float2Int, 0, usb_out, 1);  //connect the float processor to the Right output
#endif


// define the overall setup() function, the function that is called once when the device is booting
void setup() {
  Serial.begin(115200);   //open the USB serial link to enable debugging messages
  delay(500);             //give the computer's USB serial system a moment to catch up.
  Serial.println("Teensy: AudioSynthWaveform_F32 example..."); //identify myself over the USB serial

  // Audio connections require memory, and the record queue
  // uses this memory to buffer incoming audio.
  AudioMemory(12);  //allocate Int16 audio data blocks
  AudioMemory_F32(10); //allocate Float32 audio data blocks

  // Enable the audio shield, no input, and enable output
  sgtl5000.enable();                   //start the audio board
  sgtl5000.volume(0.5);                //volume can be 0.0 to 1.0.  0.5 seems to be the usual default.

  osc1.begin(1.0, 440.0, 0);           // run main oscillator at nominal amplitude, note A4, sine wave
  lfo1.begin(1.0, 0.25, 1);            // run lfo at nominal amplitude, 0.25hz (so 4 second period), saw wave
  osc1.pitchModAmount(3.0);            // Set the amount of pitch modulation to be around 3 octaves
  
} //end setup()


unsigned long curTime_millis = 0;
unsigned long lastMemUpdate_millis=0;

// define the loop() function, the function that is repeated over and over for the life of the device
void loop() {
  
  printCPUandMemoryUsage(&Serial);
} //end loop();


void printCPUandMemoryUsage(Stream *s) {
  //print status information to the Serial port
  curTime_millis = millis(); //what time is it right now
  if ((curTime_millis - lastMemUpdate_millis) < 0) lastMemUpdate_millis=0;  //handle case where millis wraps around!
  
  if ((curTime_millis - lastMemUpdate_millis) > 2000) {  // print a summary of the current & maximum usage
    s->print("Usage/Max: ");
    s->print("oscillator CPU = "); s->print(osc1.processorUsage()); s->print("/"); s->print(osc1.processorUsageMax());s->print(", ");
    s->print("LFO CPU = "); s->print(lfo1.processorUsage()); s->print("/"); s->print(lfo1.processorUsageMax());s->print(", ");
    s->print("all CPU = " ); s->print(AudioProcessorUsage()); s->print("/");  s->print(AudioProcessorUsageMax());s->print(", ");
    s->print("Int16 Mem = ");s->print(AudioMemoryUsage()); s->print("/"); s->print(AudioMemoryUsageMax());s->print(", ");
    s->print("Float Mem = ");s->print(AudioMemoryUsage_F32());s->print("/"); s->print(AudioMemoryUsageMax_F32()); s->print(", ");
    s->println();
    lastMemUpdate_millis = curTime_millis; //we will use this value the next time around.
  } 
};

