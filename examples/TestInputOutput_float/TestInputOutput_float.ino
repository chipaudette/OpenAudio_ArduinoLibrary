

#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h> 


#define USE_F32_IO 1

AudioControlSGTL5000    sgtl5000_1;

#if USE_F32_IO


#if USE_CODEC_IN

#else    // Use Sine Input
AudioSynthWaveformSine_F32 sine1;
AudioConvert_F32toI16   float2Int1;  //,  float2Int2;
AudioOutputI2S          i2sOut;
//AudioOutputI2S_OA_F32    i2sOut;

/*
//Make all of the audio connections
AudioConnection         patchCord1(i2s_in, 0, int2Float1, 0);
AudioConnection         patchCord2(i2s_in, 1, int2Float2, 0);
AudioConnection_F32     patchCord10(int2Float1, 0, gain1, 0);
AudioConnection_F32     patchCord11(int2Float2, 0, gain2, 0);
* */


AudioConnection_F32 pc1(sine1, 0, float2Int1, 0);


AudioConnection_F32     patchCord12(float2Int1, 0, i2sOut, 0); 
//AudioConnection_F32     patchCord13(gain2, 0, float2Int2, 0);
//AudioConnection_F32         patchCord20(sine1, 0, i2sOut, 0);
//AudioConnection_F32         patchCord21(sine1, 0, i2sOut, 1);

//AudioConnection_F32     patchCord12(gain1, 0, i2sOut, 0); 
//AudioConnection_F32     patchCord13(gain2, 0, i2sOut, 1);
/*AudioInputI2S           i2s_in;
AudioConvert_I16toF32   int2Float1,  int2Float2;
AudioEffectGain_F32     gain1,       gain2;
* */
void setup(void) {
  Serial.begin(1);   delay(1000);
  Serial.println("Open Audio Test Input and Output"); 
 
  AudioMemory(10);
  AudioMemory_F32(10);

  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);

 sine1.frequency(300.0);
 sine1.amplitude(0.01f);
 sine1.begin();


/*
  //sgtl5000_1.adcHighPassFilterEnable();  //LOW OUTPUT NO WHINEY NOISE, gain has no effect---Direct path??
  //reduces noise.  https://forum.pjrc.com/threads/27215-24-bit-audio-boards?p=78831&viewfull=1#post78831
  sgtl5000_1.adcHighPassFilterDisable();  gain1.setGain_dB(40); // NOISE WITH WHINEY PITCH
  
  gain2.setGain_dB(40); 
 */
 
 
} 

void loop() {
}
