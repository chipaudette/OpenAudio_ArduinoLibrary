/* TestFIRGeneralLarge5.ino  Bob Larkin 24 May 2020
 * This is a test of the FIRGeneralLarge block for Teensy Audio,
 * that has two new elements:
 *   * The FIR filter is specified by an arbitrary frequency response
 *   * The number of FIR taps is not restricted in number or in even/odd.
 *
 * This test program passes a signal from the SGTL5000 CODEC through
 * the FIR filter and back out through the CODEC.
 * This version is for the Chip Audette _F32 Library. A separate version
 * with adjustments, is for the Teensy Audio Library with I16 data types
 * called AudioFilterFIRGeneral_I16.
 */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "DSP_TeensyAudio_F32.h"

// Input as I16 and convert to be Teensy 4 compatible
AudioInputI2S             i2sIn;
AudioConvert_I16toF32     convertItoF1;
AudioFilterFIRGeneral_F32 firg1;
AudioConvert_F32toI16     convertFtoI1;
AudioOutputI2S            i2sOut;
AudioConnection        patchCord1(i2sIn,        0, convertItoF1, 0);   
AudioConnection_F32    patchCord5(convertItoF1, 0, firg1,        0);
AudioConnection_F32    patchCord9(firg1,        0, convertFtoI1, 0);
AudioConnection        patchCordD(convertFtoI1, 0, i2sOut,       0);   
AudioControlSGTL5000   codec;

float32_t dbA[2000];
float32_t workSpace[4128];
float32_t equalizeCoeffs[4000];
int i;

void setup(void) {
  uint16_t nFIR = 4;
  float32_t sidelobes = 45.0f;
  AudioMemory(5);
  AudioMemory_F32(10);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test FIR General routine for F32 ***");

  codec.enable();
  codec.inputSelect(AUDIO_INPUT_LINEIN); 
  codec.adcHighPassFilterDisable();
  codec.lineInLevel(2);
  codec.volume(0.8);

// Un-comment one of the following, or use the direct Coefficient load that follows

// Test case for odd nFIR from Burris and Parks Example 3.1 with Kaiser Window
//   nFIR = 21;  sidelobes = 45.0f;
//   dbA[0]=0.0;    dbA[1]=0.0;    dbA[2]=0.0;    dbA[3]=0.0;    dbA[4]=0.0;  dbA[5]=0.0;
//   dbA[6]=-140.0; dbA[7]=-140.0; dbA[8]=-140.0; dbA[9]=-140.0; dbA[10]=-140.0;

// Test case for even nFIR from Burris and Parks Example 3.2
//   nFIR = 20;  sidelobes = 45.0f;
//   dbA[0]=0.0;    dbA[1]=0.0;    dbA[2]=0.0;    dbA[3]=0.0; dbA[4]=0.0; dbA[5]=-120.0;
//   dbA[6]=-120.0; dbA[7]=-120.0; dbA[8]=-120.0; dbA[9]=-120.0;

// The next example tests extremely large FIR arrays and *only for Teensy 4*
// Narrow band "CW" filter, tried with 60 dB sidelobes (60 to 80 or 70 to 72
  nFIR = 3999;  sidelobes = 60;
  for(i=0; i<2000; i++) {
      if (i<70 || i>72) dbA[i] = -120.0f;
      else dbA[i] = 0.0f;
  }

  // Initialize the FIR filter design and run
  uint16_t eq = firg1.FIRGeneralNew(&dbA[0], nFIR, &equalizeCoeffs[0], sidelobes, &workSpace[0]);
  if (eq == ERR_EQ_BANDS)
    Serial.println("FIR General failed: Invalid number of frequency points.");
  else if (eq == ERR_EQ_SIDELOBES)
    Serial.println("FIR General failed: Invalid sidelobe specification.");
  else if (eq == ERR_EQ_NFIR)
    Serial.println("FIR General failed: Invalid number of FIR coefficients.");
  else
    Serial.println("FIR General initialized successfully.");
}

void loop(void) {
} 
