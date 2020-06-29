/* TestFIRGeneralLarge4.ino  Bob Larkin 24 May 2020
 * Test the generation of FIR filters and obtaining their
 * responses.  See TestFIRGeneralLarge5.ino to run the filters.
 * This is a test of the AudioFilterFIR_F32 for Teensy Audio.
  */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "DSP_TeensyAudio_F32.h"

AudioInputI2S               i2s1;   // Creates timing
AudioFilterFIRGeneral_F32   firg1;

float32_t dbA[2000];
float32_t workSpace[4128];
float32_t equalizeCoeffs[4000];
int i, k;
float32_t dBResponse[5000];  // Extreme size, can show lots of detail!

// Following is based on Rabiner, McGonegal and Paul, FWFIR test case, but has
// had the windowing effect divided out to show their basic filter response.
float32_t hpCoeff[55]={
-0.003643388f, -0.007195844f,  0.012732424f, -0.007796006f, -0.004276372f,
 0.013760401f, -0.012262941f,  0.000000304f,  0.013553423f, -0.016818445f,
 0.005786355f,  0.011693321f, -0.021220577f,  0.013364335f, 0.007566100f,
-0.025227439f,  0.023410856f, -0.000000305f, -0.028612852f, 0.037841329f,
-0.014052131f, -0.031182658f,  0.063661835f, -0.046774701f, -0.032787389f,
 0.151364865f, -0.257518316f,  0.300000700f, -0.257518316f, 0.151364865f,
-0.032787389f, -0.046774701f,  0.063661835f, -0.031182658f, -0.014052131f,
 0.037841329f, -0.028612852f, -0.000000305f,  0.023410856f, -0.025227439f,
 0.007566100f,  0.013364335f, -0.021220577f,  0.011693321f, 0.005786355f,
-0.016818445f,  0.013553423f,  0.000000304f, -0.012262941f, 0.013760401f,
-0.004276372f, -0.007796006f,  0.012732424f, -0.007195844f, -0.003643388};

void setup(void) {
  uint16_t nFIR = 4;
  float32_t sidelobes = 45;
  AudioMemory(5);
  AudioMemory_F32(10);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test Response of FIR General routine for F32 ***");

// Un-comment one of the following, or use the direct Coefficient load that follows

// Test case for odd nFIR from Burris and Parks Example 3.1
   nFIR = 21;  sidelobes = 0.0;  // No window
   dbA[0]=0.0;    dbA[1]=0.0;    dbA[2]=0.0;    dbA[3]=0.0;    dbA[4]=0.0;  dbA[5]=0.0;
   dbA[6]=-140.0; dbA[7]=-140.0; dbA[8]=-140.0; dbA[9]=-140.0; dbA[10]=-140.0;

// Test case for even nFIR from Burris and Parks Example 3.2
//  nFIR = 20;  sidelobes = 0.0;
//  dbA[0]=0.0;    dbA[1]=0.0;    dbA[2]=0.0;    dbA[3]=0.0; dbA[4]=-120.0; dbA[5]=-120.0;
//  dbA[6]=-120.0; dbA[7]=-120.0; dbA[8]=-120.0; dbA[9]=-120.0;

// Test case for odd nFIR from Burris and Parks Example 3.1 with Kaiser Window
//   nFIR = 21;  sidelobes = 45.0f;
//   dbA[0]=0.0;    dbA[1]=0.0;    dbA[2]=0.0;    dbA[3]=0.0;    dbA[4]=0.0;  dbA[5]=0.0;
//   dbA[6]=-140.0; dbA[7]=-140.0; dbA[8]=-140.0; dbA[9]=-140.0; dbA[10]=-140.0f;

// Test case for even nFIR from Burris and Parks Example 3.2
//   nFIR = 20;  sidelobes = 45.0f;
//   dbA[0]=0.0;    dbA[1]=0.0;    dbA[2]=0.0;    dbA[3]=0.0; dbA[4]=0.0; dbA[5]=-120.0;
//   dbA[6]=-120.0; dbA[7]=-120.0; dbA[8]=-120.0; dbA[9]=-120.0;

// Rabiner, McGonegal and Paul, FWFIR test case, HP Kaiser SL=60, fco=0.35
//  nFIR = 55;  sidelobes = 60.0;
//  for(i=0; i<28; i++) {
//     if (i<20) dbA[i] = -120.0f;
//     else dbA[i] = 0.0f;
//  }

// The next 3 examples are tests of extremely large FIR arrays and only for Teensy 4
// Generate a response curve for our "octave" equalizer, tried with 30 dB sidelobes
//  nFIR = 3999;  sidelobes = 30.0f;
//  dbA[0] = 12.0f;  // Avoid log(0)
//  for(i=1; i<2000; i++)
//      dbA[i] = 12.0f*cosf(10.0f*log10f(11.02925f*(float32_t)i));

// Generate a slowly changing response, up 12 dB at bass and treble and -12 dB
// for mid-frequencies.  Scaled to "octaves" by the form cos(log(f))
//  nFIR = 3999;  sidelobes = 30.0f;
//  dbA[0] = 12.0f;
//  dbA[1] = 12.0f;
//  for(i=2; i<2000; i++)
//      dbA[i] = -12.0f*cosf(2.2f*log10f(11.02925f*(float32_t)i));

// Narrow band "CW" filter, tried with 60 dB sidelobes (60 to 80 or 70 to 72
//   nFIR = 3999;  sidelobes = 60;
//   for(i=0; i<2000; i++) {
//      if (i<70 || i>72) dbA[i] = -120.0f;
//      else dbA[i] = 0.0f;
//  }

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

// Load (if not commented) non-windowed HP filter coefficients, test case:
// firg1.LoadCoeffs(55, hpCoeff, workSpace);  // Direct load of FIR coefficients, already calculated

  Serial.println("FIR Coeffs");
  for(i=0; i<nFIR; i++)
      Serial.println(equalizeCoeffs[i], 7);
  Serial.println("");

  // Get frequency response in dB for 5000 points, uniformly spaced from 0 to 22058 Hz
  // This is chosen at 5000 for test.  Normally, a smaller value,
  // perhaps 500, would be adequate.
  firg1.getResponse(5000, dBResponse);
  Serial.println("Freq Hz, Response dB");
  for(int m=0; m<5000; m++) {       // Print the response in Hz, dB, suitable for a spread sheet
       // Uncomment/comment next 3 for print/no print
       Serial.print((float32_t)m * 22058.5f / 5000.0f);
       Serial.print(",");
       Serial.println(dBResponse[m]);
  }
}

void loop(void) {
} 
