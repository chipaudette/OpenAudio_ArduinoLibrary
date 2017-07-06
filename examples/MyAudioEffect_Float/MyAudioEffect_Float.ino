/*
   BasicGain

   Created: Chip Audette, Dec 2016
   Purpose: Be a blank canvas for adding your own floating-point audio processing.

   Uses Teensy Audio Adapter.
   Assumes microphones (or whatever) are attached to the LINE IN (stereo)
   Listens  potentiometer mounted to Audio Board to provde a control signal.

   MIT License.  use at your own risk.
*/

//These are the includes from the Teensy Audio Library
#include <Audio.h>      //Teensy Audio Library
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include <OpenAudio_ArduinoLibrary.h> //for AudioConvert_I16toF32, AudioConvert_F32toI16, and AudioEffectGain_F32
#include "AudioEffectMine_F32.h";

//create audio library objects for handling the audio
AudioControlSGTL5000    sgtl5000_1;    //controller for the Teensy Audio Board
AudioInputI2S           i2s_in;        //Digital audio *from* the Teensy Audio Board ADC.  Sends Int16.  Stereo.
AudioConvert_I16toF32   int2Float1, int2Float2;    //Converts Int16 to Float.  See class in AudioStream_F32.h
AudioConvert_F32toI16   float2Int1, float2Int2;    //Converts Float to Int16.  See class in AudioStream_F32.h
AudioEffectMine_F32     effect1, effect2;  //This is your own algorithms
AudioOutputI2S          i2s_out;       //Digital audio *to* the Teensy Audio Board DAC.  Expects Int16.  Stereo

//Make all of the audio connections
AudioConnection         patchCord1(i2s_in, 0, int2Float1, 0);    //connect the Left input to the Left Int->Float converter
AudioConnection         patchCord2(i2s_in, 1, int2Float2, 0);    //connect the Right input to the Right Int->Float converter
AudioConnection_F32     patchCord10(int2Float1, 0, effect1, 0);    //Left.  makes Float connections between objects
AudioConnection_F32     patchCord11(int2Float2, 0, effect2, 0);    //Right.  makes Float connections between objects
AudioConnection_F32     patchCord12(effect1, 0, float2Int1, 0);    //Left.  makes Float connections between objects
AudioConnection_F32     patchCord13(effect2, 0, float2Int2, 0);    //Right.  makes Float connections between objects
AudioConnection         patchCord20(float2Int1, 0, i2s_out, 0);  //connect the Left float processor to the Left output
AudioConnection         patchCord21(float2Int2, 0, i2s_out, 1);  //connect the Right float processor to the Right output

// which input on the audio shield will be used?
const int myInput = AUDIO_INPUT_LINEIN;
//const int myInput = AUDIO_INPUT_MIC;

//I have a potentiometer on the Teensy Audio Board
#define POT_PIN A1  //potentiometer is tied to this pin

// define the setup() function, the function that is called once when the device is booting
void setup() {
  Serial.begin(115200);   //open the USB serial link to enable debugging messages
  delay(500);             //give the computer's USB serial system a moment to catch up.
  Serial.println("OpenAudio_ArduinoLibrary: MyAudioEffect_Float...");

  // Audio connections require memory
  AudioMemory(10);      //allocate Int16 audio data blocks
  AudioMemory_F32(10);  //allocate Float32 audio data blocks

  // Enable the audio shield, select input, and enable output
  sgtl5000_1.enable();                   //start the audio board
  sgtl5000_1.inputSelect(myInput);       //choose line-in or mic-in
  sgtl5000_1.volume(0.8);                //volume can be 0.0 to 1.0.  0.5 seems to be the usual default.
  sgtl5000_1.lineInLevel(10, 10);        //level can be 0 to 15.  5 is the Teensy Audio Library's default
  sgtl5000_1.adcHighPassFilterDisable(); //reduces noise.  https://forum.pjrc.com/threads/27215-24-bit-audio-boards?p=78831&viewfull=1#post78831

  // setup any other other features
  pinMode(POT_PIN, INPUT); //set the potentiometer's input pin as an INPUT

} //end setup()


// define the loop() function, the function that is repeated over and over for the life of the device
void loop() {
  //choose to sleep ("wait for interrupt") instead of spinning our wheels doing nothing but consuming power
  asm(" WFI");  //ARM-specific.  Will wake on next interrupt.  The audio library issues tons of interrupts, so we wake up often.

  //service the potentiometer...if enough time has passed
  servicePotentiometer(millis());

  //update the memory and CPU usage...if enough time has passed
  printMemoryAndCPU(millis());
} //end loop()


//servicePotentiometer: listens to the blue potentiometer and sends the new pot value
//  to the audio processing algorithm as a control parameter
void servicePotentiometer(unsigned long curTime_millis) {
  static unsigned long updatePeriod_millis = 100; //how many milliseconds between updating the potentiometer reading?
  static unsigned long lastUpdate_millis = 0;
  static float prev_val = 0;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?

    //read potentiometer
    float val = float(analogRead(POT_PIN)) / 1024.0; //0.0 to 1.0
    val = 0.1 * (float)((int)(10.0 * val + 0.5)); //quantize so that it doesn't chatter

    //add code here to change the potentiometer value to something useful (like gain_dB?)
    // ..... add code here if you'd like ......

    //send the potentiometer value to your algorithm as a control parameter
    if (abs(val - prev_val) > 0.05) { //is it different than befor?
      Serial.print("Sending new value to my algorithms: "); Serial.println(val);
      effect1.setUserParameter(val);   effect2.setUserParameter(val);
    }
    prev_val = val;  //use the value the next time around
  } // end if
} //end servicePotentiometer();


void printMemoryAndCPU(unsigned long curTime_millis) {
  static unsigned long updatePeriod_millis = 2000; //how many milliseconds between updating gain reading?
  static unsigned long lastUpdate_millis = 0;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?
    Serial.print("CPU: Usage, Max: ");
    Serial.print(AudioProcessorUsage());
    Serial.print(", ");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("    ");
    Serial.print("Int16 Memory: ");
    Serial.print(AudioMemoryUsage());
    Serial.print(", ");
    Serial.print(AudioMemoryUsageMax());
    Serial.print("    ");
    Serial.print("Float Memory: ");
    Serial.print(AudioMemoryUsage_F32());
    Serial.print(", ");
    Serial.print(AudioMemoryUsageMax_F32());
    Serial.println();
    lastUpdate_millis = curTime_millis; //we will use this value the next time around.
  }
}

