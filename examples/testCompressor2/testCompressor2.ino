/* TestCompressor2.ino  Bob Larkin 23 January 2021
 *
 * Test of AudioEffectCompressor2_F32
 * See AudioEffectCompressor2_F32.h for much detail and explanation.
 * Choice of test signals is a single sine wave, a random sequence
 * of sine waves of varying frequency and amplitude, a power
 * sweep or a pulse of sine wave to see transient behavior.
 *
 * This version is for the Chip Audette OpenAudio_F32 Library. and
 * thus has that interface structure.
 *
 * NOTE: As  of 23 January 2021, the compressor AudioEffectWDRC2_F32.h
 * was not finalized and could change in detail.  Use here with
 * this in mind.
 */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"

#define RANDOM 1
#define POWER_SWEEP 2
#define PULSE 3
#define ALTERNATE 4
// Edit in one of the last four, here:
#define SIGNAL_SOURCE RANDOM

AudioSynthWaveformSine_F32  sine1;       // Test signal
AudioEffectCompressor2_F32  compressor1; // Audio Compressor
AudioEffectGain_F32         gain0;       // Sets volume sent to output
AudioEffectGain_F32         gain1;       // Sets the same
AudioOutputI2S_F32          i2sOut;
AudioConnection_F32         patchCord1(sine1,       0, compressor1, 0);
AudioConnection_F32         patchCord2(compressor1, 0, gain0,       0);
AudioConnection_F32         patchCord3(compressor1, 0, gain1,       0);
AudioConnection_F32         patchCord4(gain0,       0, i2sOut,      0);
AudioConnection_F32         patchCord5(gain1,       0, i2sOut,      1);
AudioControlSGTL5000        sgtl5000_1;

uint16_t count17, count27;
float level = 0.05f;

void setup(void) {
  AudioMemory(50);
  AudioMemory_F32(100);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test Compressor2 Gain Compressor  **");
  
  count17 = 0;
  count27 = 0;
  sine1.frequency(1000.0f);
  sine1.amplitude(0.05f);
  // CAUTION - If using ears on the output, adjust the following two carefully
  gain0.setGain_dB(-25.0f);  // Consider (-50.0f);
  gain1.setGain_dB(-25.0f);  // Consider (-50.0f);
  sgtl5000_1.enable();
  
  int16_t delaySize = 256;     // Any power of 2, i.e., 256, 128, 64, etc.
  compressor1.setDelayBufferSize(delaySize);

  // ***  Here are four sample compressor curves. ***
  // Select by number here, and re-compile.
#define COMPRESSOR_CURVE 1

#if COMPRESSOR_CURVE == 1
  // Specify arbitrary 5-segment compression curve.  An example of specifying
  // compressionCurve.  See AudioEffectCompressor2_F32.h for more details.
  struct compressionCurve crv = { -2.0f, 0.0f,           // margin, offset
     {0.0f, -10.0f, -20.0f, -30.0f, -1000.0f},           // kneeDB[] 
     {  100.0f,  2.5f,   1.5f,     1.0f,      1.0f} };    // compressionRatio
  compressor1.setCompressionCurve(&crv);
  compressor1.begin();
#endif
 
  AudioEffectCompressor2_F32 *pc1 = &compressor1; // Needed for any *macro* defined curve.
                                             // Defines pointer to compressor1, called pc1
 
#if COMPRESSOR_CURVE == 2
  // Sample of a limiter at -3 dB out for highest 15 dB, limiter macro
  limiterBegin(pc1, -3.0f, -15.0f);  // pc1 is a pointer to compressor1 object

#elif COMPRESSOR_CURVE == 3
  // Another one, a basic compressor curve:
  //              (pobject, linearInDB, compressionRatioDB) <- macro parameters 
  basicCompressorBegin(pc1, -25.0f, 2.0);

#elif COMPRESSOR_CURVE == 4
  // And one using a high gain region as a squelch (but no clicks or pops)
  //          (squelchIndB, linearInDB, compressionIndB, compressionRatioDB) <- macro parameters
  squelchCompressorBegin(pc1, -40.0f, -25.0f, -10.0f, 1.5f);
#endif
}

void loop(void)
   {
   static uint16_t kk;

#if SIGNAL_SOURCE == RANDOM
   /* To give an audio signal with interest, we alter the frequency
    * every 17 blocks (49 msec) and alter the level every 27 b;ocks
    * (78.4 msec)  The pattern keeps changing to be more interesting
    * Janet thinks it is aliens. */

    delay(3);
    // Serial.print("  CurInDB= ");  Serial.print( compressor1.getCurrentInputDB(), 3);
    // Serial.print("  CurrentGainDB= ");  Serial.println( compressor1.getCurrentGainDB(), 3);
    // Serial.print("Maximum Input = ");  Serial.println(compressor1.getvInMaxDB());
    if(count17++ == 17)
       {
       // Put a delay in, like between words
       if(randUniform() < 0.03)
          delay( (int)(1500.0*randUniform()) );
       count17 = 0;
       float ff = 350.0f + 700.0f*sqrtf( randUniform() );
       sine1.frequency(ff);  //Serial.println(ff);
       }
    if(count27++ == 27)
       {
       count27 = 0;
       level = 1.0f*powf( randUniform(), 2 );  // 0 to 1, emphasizing 0 end
       sine1.amplitude(level);
       }

#elif SIGNAL_SOURCE == POWER_SWEEP
    if(count17++ == 17)
       {
       count17 = 0;
       level *= 1.0592537f;   // 0.5 dB 
       delay(200);
       if(level > 1.0f)
          {
          level=0.001f; 
          delay(500);
          }
       Serial.print( compressor1.getCurrentInputDB(), 3);
       Serial.print(",");  Serial.println( compressor1.getCurrentGainDB(), 3);
       sine1.amplitude(level);
       }

#elif SIGNAL_SOURCE == PULSE
      // A pulse, repeats every 3 minutes or so
      delay(3);
      if(count17 == 5)     sine1.amplitude(0.0f);  // Settling
      else if(count17 == 498)   compressor1.printOn(true); //record it
      else if(count17 == 500)   sine1.amplitude(0.03f);
      else if(count17 == 510)   sine1.amplitude(0.0f);
      else if(count17 == 700)   compressor1.printOn(false);
      // or build your own transient test pulse here
      count17++;
#endif
   }

/* randUniform()  -  Returns random number, uniform on (0, 1)
 * The "Even Quicker" uniform random sample generator from D. E. Knuth and
 * H. W. Lewis and described in Chapter 7 of "Numerical Receipes in C",
 * 2nd ed, with the comment "this is about as good as any 32-bit linear
 * congruential generator, entirely adequate for many uses."
 */
#define FL_ONE  0X3F800000
#define FL_MASK 0X007FFFFF
float randUniform(void)
   {
   static uint32_t idum = 12345;
   union {
     uint32_t  i32;
     float f32;
     } uinf;

   idum = (uint32_t)1664525 * idum + (uint32_t)1013904223;
   // return (*(float *)&it);  // Cute convert to float but gets compiler warning
   uinf.i32 = FL_ONE | (FL_MASK & idum);   // So do the same thing with a union

   return uinf.f32 - 1.0f;
   }
