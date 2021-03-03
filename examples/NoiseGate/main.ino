#include <Audio.h>
// the next line can be skipped if more perfomance is needed
#define NOISEGATE_EXTENDEDINFO
#include <OpenAudio_ArduinoLibrary.h>
#include <Arduino.h>

AudioInputI2S_F32 i2sAudioIn1;
AudioOutputI2S_F32 i2sAudioOut1;
AudioEffectNoiseGate_F32 noiseGate;

AudioConnection_F32 patchCordL1(i2sAudioIn1, 0, noiseGate, 0);
AudioConnection_F32 patchCordL2(noiseGate, 0, i2sAudioOut1, 0);
AudioConnection_F32 patchCordR1(i2sAudioIn1, 1, i2sAudioOut1, 1);



#define LED_RED 0
#define LED_YELLOW 1
#define LED_GREEN 2

//The setup function is called once when the system starts up
void setup(void)
{
  //Start the USB serial link (to enable debugging)
  Serial.begin(115200);
  delay(500);
  Serial.println("Setup starting...");

  //Allocate dynamically shuffled memory for the audio subsystem
  AudioMemory(20);
  AudioMemory_F32(20);
 
  //Put your own setup code here
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);

  // setup the noise gate. it is probaly a good idea to filter the incoming signal @20 Hz, maybe this colud be added in the future
  noiseGate.setOpeningTime(0.02f);
  noiseGate.setClosingTime(0.05f);
  noiseGate.setHoldTime(0.1);
  noiseGate.setThreshold(-40);

  //End of setup
  Serial.println("Setup complete.");
};

//After setup(), the loop function loops forever.
//Note that the audio modules are called in the background.
//They do not need to be serviced by the loop() function.
float lastVal = 0;
void loop(void)
{

  bool thresTrigger = noiseGate.infoIsOpen();
  digitalWrite(LED_RED, !thresTrigger);
  digitalWrite(LED_GREEN, thresTrigger);
  digitalWrite(LED_YELLOW, noiseGate.infoIsOpeningOrClosing());
  
};
