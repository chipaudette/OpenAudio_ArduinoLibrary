/* TestEqualizer1Audio.ino  Bob Larkin 18 May 2020
 * This is a test of the Filter Equalizer using the Audette OpenAudio_ArduinoLibrary.
 * Runs two different equalizers, switching every 5 seconds to demonstrate
 * dynamic filter changing.
 */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "DSP_TeensyAudio_F32.h"
#include <Audio.h>

// Signals from ADC to Equalizer to DAC using Teensy Audio Adaptor
AudioInputI2S            i2sIn;
AudioConvert_I16toF32    convertItoF1;
AudioConvert_I16toF32    convertItoF2;
AudioFilterEqualizer_F32 equalize1;
AudioFilterEqualizer_F32 equalize2;
AudioConvert_F32toI16    convertFtoI1;
AudioConvert_F32toI16    convertFtoI2;
AudioOutputI2S           i2sOut;
AudioConnection        patchCord1(i2sIn,        0, convertItoF1, 0);   
AudioConnection        patchCord3(i2sIn,        1, convertItoF2, 0);
AudioConnection_F32    patchCord5(convertItoF1, 0, equalize1,    0);
AudioConnection_F32    patchCord7(convertItoF2, 0, equalize2,    0);
AudioConnection_F32    patchCord9(equalize1,    0, convertFtoI1, 0);
AudioConnection_F32    patchCordB(equalize2,    0, convertFtoI2, 0);
AudioConnection        patchCordD(convertFtoI1, 0, i2sOut,       0);   
AudioConnection        patchCordF(convertFtoI2, 0, i2sOut,       1);
AudioControlSGTL5000   codec;

// Some sort of octave band equalizer as a one alternative, 10 bands
float32_t fBand1[] = {    40.0, 80.0, 160.0, 320.0, 640.0, 1280.0, 2560.0, 5120.0, 10240.0, 22058.5};
float32_t dbBand1[] = {10.0,  2.0, -2.0,  -5.0, -2.0,  -4.0,   -10.0,   6.0,    10.0,    -100};

// To contrast, put a strange bandpass filter as an alternative, 5 bands
float32_t fBand2[] = {    300.0, 500.0, 800.0, 1000.0, 22058.5};
float32_t dbBand2[] = {-60.0,  0.0,  -20.0,   0.0,  -60.0};

float32_t equalizeCoeffs[200];
int16_t k = 0;

void setup(void) {
  AudioMemory(5); 
  AudioMemory_F32(10);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test Audio Equalizer ***");
  codec.enable();
  codec.inputSelect(AUDIO_INPUT_LINEIN); 
  codec.adcHighPassFilterDisable(); // necessary to suppress noise
  codec.lineInLevel(2);
  codec.volume(0.8);

  // Initialize the equalizer with 10 bands, fBand1[] 199 FIR coefficients
  // -65 dB sidelobes, 16384 Max coefficient value
  uint16_t eq = equalize1.equalizerNew(10, &fBand1[0], &dbBand1[0], 30, &equalizeCoeffs[0], 60.0);
  if (eq == ERR_EQ_BANDS)
    Serial.println("Equalizer failed: Invalid number of frequency bands.");
  else if (eq == ERR_EQ_SIDELOBES)
    Serial.println("Equalizer failed: Invalid sidelobe specification.");
  else if (eq == ERR_EQ_NFIR)
    Serial.println("Equalizer failed: Invalid number of FIR coefficients.");
  else
    Serial.println("Equalizer initialized successfully.");
    
  eq = equalize2.equalizerNew(10, &fBand1[0], &dbBand1[0], 30, &equalizeCoeffs[0], 60.0);
  if (eq == ERR_EQ_BANDS)
    Serial.println("Equalizer failed: Invalid number of frequency bands.");
  else if (eq == ERR_EQ_SIDELOBES)
    Serial.println("Equalizer failed: Invalid sidelobe specification.");
  else if (eq == ERR_EQ_NFIR)
    Serial.println("Equalizer failed: Invalid number of FIR coefficients.");
  else
    Serial.println("Equalizer initialized successfully.");
  // for (k=0; k<200; k++) Serial.println(equalizeCoeffs[k]);
}

void loop(void) {
#if 0  // Change between two filters every 10 seconds.
  
  // To run with just the 10-band equalizer, comment out the entire loop with "/* ... */"

  if(k == 0) {
     k = 1;
     equalize1.equalizerNew(10, &fBand1[0], &dbBand1[0], 200, &equalizeCoeffs[0], 65.0);
     equalize2.equalizerNew(10, &fBand1[0], &dbBand1[0], 200, &equalizeCoeffs[0], 65.0);
  }
  else  {  // Swap back and forth
      k = 0;
      equalize1.equalizerNew(5, &fBand2[0], &dbBand2[0], 100, &equalizeCoeffs[0], 40.0);
      equalize2.equalizerNew(5, &fBand2[0], &dbBand2[0], 100, &equalizeCoeffs[0], 40.0);
  }
#endif
    delay(10000);   // Interrupts will keep the audio going
    Serial.print("Proc = ");
    Serial.print(AudioProcessorUsage());
    Serial.print(" (");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("),  Mem = ");
    Serial.print(AudioMemoryUsage());
    Serial.print(" (");
    Serial.print(AudioMemoryUsageMax());
    Serial.println(")");
    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
}
