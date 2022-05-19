/* Spectral Noise reduction test program.
 *  
 *  The example takes sound in from both the I2S and USB of the Teensy/audio-daughtercard,
 *  processes it, and sends it back out the I2S/headphone ports.
 *  Every 10 seconds it switches from Spectral processing to data-passthrough and back,
 *  to aid comparison.
 *  Some information is printed on the serial monitor.
 *  
 *  This example requires the Teensy Board 'USB Type' in the Arduino Tools menu to be set
 *  to a type that includes 'Audio', and ideally 'Serial' as well. Tested with
 *  'Serial+MIDI+Audio'.
 *  If you do not set 'Audio', you will get a compliation errors similar to:
 *      "... OpenAudio_ArduinoLibrary/USB_Audio_F32.h: In member function 'virtual void AudioOutputUSB_F32::update()':"
 *      "... OpenAudio_ArduinoLibrary/USB_Audio_F32.h:139:3: error: 'usb_out' was not declared in this scope"
 *
 * MIT License.  use at your own risk.
 */

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include "USB_Audio_F32.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputI2S_F32        audioInI2S1;    //xy=117,343
AudioInputUSB_F32        audioInUSB1;    //xy=146,397
AudioMixer4_F32          input_mixer;       //xy=370,321
AudioSpectralDenoise_F32    Spectral;        //xy=852,250
AudioMixer4_F32          output_mixer; //xy=993,296
AudioSwitch4_OA_F32      processing_switch;
AudioOutputI2S_F32       audioOutI2S1;   //xy=1257,367
AudioOutputUSB_F32       audioOutUSB1;   //xy=1261,418

//Inputs - mixed into one stream
AudioConnection_F32      patchCord1(audioInI2S1, 0, input_mixer, 0);
AudioConnection_F32      patchCord2(audioInUSB1, 0, input_mixer, 1);

//route through a switch, so we can switch Spectral in/out
AudioConnection_F32      patchCord3(input_mixer, 0, processing_switch, 0);

//First route is direct - direct to the output mixer
AudioConnection_F32      patchCord4(processing_switch, 0, output_mixer, 0);

//Second route is through Spectral to the output mixer
AudioConnection_F32      patchCord5(processing_switch, 1, Spectral, 0);
AudioConnection_F32      patchCord6(Spectral, 0, output_mixer, 1);

//And finally output the mixer to the output channels
AudioConnection_F32      patchCord7(output_mixer, 0, audioOutI2S1, 0);
AudioConnection_F32      patchCord8(output_mixer, 0, audioOutI2S1, 1);
AudioConnection_F32      patchCord9(output_mixer, 0, audioOutUSB1, 0);
AudioConnection_F32      patchCord10(output_mixer, 0, audioOutUSB1, 1);

AudioControlSGTL5000     sgtl5000_1;     //xy=519,146
// GUItool: end automatically generated code

AudioSettings_F32 audio_settings(AUDIO_SAMPLE_RATE_EXACT, AUDIO_BLOCK_SAMPLES);

int current_cycle = 0;  //Choose how we route the audio - to process or not

static void spectralSetup(void){
  //Use a 1024 FFT in this example
  if (Spectral.setup(audio_settings, 1024) < 0 ) {
    Serial.println("Failed to setup Spectral");
  } else {
    Serial.println("Spectral setup OK");    
  }
  Serial.flush();
}

//The setup function is called once when the system starts up
void setup(void) {
  //Start the USB serial link (to aid debugging)
  Serial.begin(115200); delay(500);
  Serial.println("Setup starting...");
  
  //Allocate dynamically shuffled memory for the audio subsystem
  AudioMemory(30);  AudioMemory_F32(30);

  Serial.println("Calling Spectral setup");
  spectralSetup();
  Serial.println("Spectral Setup done");
  sgtl5000_1.enable();
  sgtl5000_1.unmuteHeadphone();
  sgtl5000_1.volume(0.5);

  //End of setup
  Serial.println("Setup complete.");
};


//After setup(), the loop function loops forever.
//Note that the audio modules are called in the background.
//They do not need to be serviced by the loop() function.
void loop(void) {
  // every 'n' seconds move to the next cycle of processing.
  if ( ((millis()/1000) % 10) == 0 ) {
    current_cycle++;
    if (current_cycle >= 2) current_cycle = 0;

    switch( current_cycle ) {
      case 0:
        Serial.println("Passthrough");
        processing_switch.setChannel(0);
        break;

      case 1:
        Serial.println("Run Spectral NR");
        processing_switch.setChannel(1);
        break;

      default:
        current_cycle = 0;  //oops - reset to start
        break;
    }
  }
  
  //Nap - we don't need to hard-spin...
  delay(1000);
};
