/*
   MixStereoToMono

   Created: Chip Audette, Jan 2017
   Purpose: Process audio mixing the Left and Right inputs into Mono.
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
AudioMixer4_F32           mixer; //mix floating point data      
AudioConvert_F32toI16     float2Int1, float2Int2;    //Converts Float to Int16.  See class in AudioStream_F32.h
AudioOutputI2S            i2s_out;            //Digital audio *to* the Teensy Audio Board DAC.  Expects Int16.  Stereo

//Do you want to use the USB audio as your input, or do you want to use i2s as your input?
#define DO_USB  0    //set to 1 to enable USB audio.  Be sure to go under the "Tools" menu and do "USB Type" -> "Audio"
#if DO_USB
  AudioInputUSB           usb_in;  
  AudioConnection         patchCord1(usb_in, 0, int2Float1, 0);
  AudioConnection         patchCord2(usb_in, 1, int2Float2, 0);
#else
  AudioConnection         patchCord1(i2s_in, 0, int2Float1, 0);   //connect the Left input to the Left Int->Float converter
  AudioConnection         patchCord2(i2s_in, 1, int2Float2, 0);   //connect the Right input to the Right Int->Float converter
#endif
AudioConnection_F32     patchCord10(int2Float1, 0, mixer, 0); //Left.  makes Float connections between objects
AudioConnection_F32     patchCord11(int2Float2, 0, mixer, 1); //Right.  makes Float connections between objects
AudioConnection_F32     patchCord12(mixer, 0, float2Int1, 0); //Left.  makes Float connections between objects
AudioConnection_F32     patchCord13(mixer, 0, float2Int2, 0); //Right.  makes Float connections between objects

//For output, you always need an i2s or else you have no clock to drive the audio subsystem.
//So, even if you're not going to use the headphone/line-out, you need these lines 
AudioConnection         patchCord20(float2Int1, 0, i2s_out, 0);  //connect the Left float processor to the Left output
AudioConnection         patchCord21(float2Int2, 0, i2s_out, 1);  //connect the Right float processor to the Right output

//Now, if you want USB output, it gets enabled here
#if DO_USB
  AudioOutputUSB          usb_out;
  AudioConnection         patchCord30(float2Int1, 0, usb_out, 0);  //connect the Left float processor to the Left output
  AudioConnection         patchCord31(float2Int2, 0, usb_out, 1);  //connect the Right float processor to the Right output
#endif


// which input on the audio shield will be used?
const int myInput = AUDIO_INPUT_LINEIN;
//const int myInput = AUDIO_INPUT_MIC;


//define a function to setup the Teensy Audio Board how I like it
void setupMyAudioBoard(void) {
  sgtl5000.enable();                   //start the audio board
  sgtl5000.inputSelect(myInput);       //choose line-in or mic-in
  sgtl5000.volume(0.8);                //volume can be 0.0 to 1.0.  0.5 seems to be the usual default.
  sgtl5000.lineInLevel(10,10);         //level can be 0 to 15.  5 is the Teensy Audio Library's default
  sgtl5000.adcHighPassFilterDisable(); //reduces noise.  https://forum.pjrc.com/threads/27215-24-bit-audio-boards?p=78831&viewfull=1#post78831
  sgtl5000.micBiasEnable(3.0);         //enable the mic bias voltage...only in AudioControlSGTL5000_Extended
}

// define the overall setup() function, the function that is called once when the device is booting
void setup() {
  Serial.begin(115200);   //open the USB serial link to enable debugging messages
  delay(500);             //give the computer's USB serial system a moment to catch up.
  Serial.println("Teensy Hearing Aid: BasicCompressor_Float..."); //identify myself over the USB serial

  // Audio connections require memory, and the record queue
  // uses this memory to buffer incoming audio.
  AudioMemory(12);  //allocate Int16 audio data blocks
  AudioMemory_F32(10); //allocate Float32 audio data blocks

  // Enable the audio shield, select input, and enable output
  setupMyAudioBoard();

  // set the gains on the mixer
  mixer.gain(0,0.5);  //set gain to 0.5 (instead of 1.0) so that the mixer will never clip due to my two-channel mix
  mixer.gain(1,0.5);  //set gain to 0.5 (instead of 1.0) so that the mixer will never clip due to my two-channel mix

} //end setup()

// define the loop() function, the function that is repeated over and over for the life of the device
unsigned long curTime_millis = 0;
unsigned long lastMemUpdate_millis=0;
void loop() {
  //print status information to the Serial port
  curTime_millis = millis(); //what time is it right now
  if ((curTime_millis - lastMemUpdate_millis) < 0) lastMemUpdate_millis=0;  //handle case where millis wraps around!
  if ((curTime_millis - lastMemUpdate_millis) > 2000) {  // print a summary of the current & maximum usage
    printCPUandMemoryUsage(&Serial);
    lastMemUpdate_millis = curTime_millis; //we will use this value the next time around.
  }

} //end loop();


void printCPUandMemoryUsage(Stream *s) {
    s->print("Usage/Max: ");
    s->print("mixer CPU = "); s->print(mixer.processorUsage()); s->print("/"); s->print(mixer.processorUsageMax());s->print(", ");
    s->print("all CPU = " ); s->print(AudioProcessorUsage()); s->print("/");  s->print(AudioProcessorUsageMax());s->print(", ");
    s->print("Int16 Mem = ");s->print(AudioMemoryUsage()); s->print("/"); s->print(AudioMemoryUsageMax());s->print(", ");
    s->print("Float Mem = ");s->print(AudioMemoryUsage_F32());s->print("/"); s->print(AudioMemoryUsageMax_F32()); s->print(", ");
    s->println();
};

