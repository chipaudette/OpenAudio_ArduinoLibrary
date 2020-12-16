/* TestWDRC2.ino  Bob Larkin 8 Dec 2020
 * 
 * Test of AudioEffectWDRC2_F32 (Wide Dynamic Range Compressor)
 * See AudioEffectWDRC2_F32.h for much detail and explanation.
 * Choice of test signals is a single sine wave, a random sequence
 * of sine waves of varying frequency and amplitude, a power
 * sweep or a pulse of sine wave to see transient behavior.
 *
 * This version is for the Chip Audette OpenAudio_F32 Library. and
 * thus has that interface structure.
 * 
 * NOTE: As  of 20 Dec 2020, the compressor AudioEffectWDRC2_F32.h
 * was not finalized and could change in detail.  Use here with
 * this in mind.
 */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioEffectWDRC2_F32.h"

#define CW 0
#define RANDOM 1
#define POWER_SWEEP 2
#define PULSE 3
// Edit in one of the last four, here:
#define SIGNAL_SOURCE PULSE 

AudioSynthWaveformSine_F32  sine1;      // Test signal
AudioPlayQueue_F32          queue0;     // Amplitude set of input
AudioMultiply_F32           mult1;
AudioEffectWDRC2_F32        compressor1; // Wide Dynamic Range Compressor
AudioFilterFIR_F32          fir;
AudioEffectGain_F32         gain0;       // Sets volume sent to output
AudioEffectGain_F32         gain1;       // Sets the same
AudioConvert_F32toI16       convert0;    // Allow integer output driver
AudioConvert_F32toI16       convert1;
AudioOutputI2S              i2sOut;
AudioConnection_F32         patchCord0(sine1,       0, mult1,       0);
AudioConnection_F32         patchCord1(queue0,      0, mult1,       1);
AudioConnection_F32         patchCord2(mult1,       0, fir,         0);
AudioConnection_F32         patchCord3(fir,         0, compressor1, 0);
AudioConnection_F32         patchCord4(compressor1, 0, gain0,       0);
AudioConnection_F32         patchCord5(fir,         0, gain1,       0);
AudioConnection_F32         patchCord6(gain0,       0, convert0,    0);
AudioConnection_F32         patchCord7(gain1,       0, convert1,    0);
AudioConnection             patchCord8(convert0,    0, i2sOut,      0);
AudioConnection             patchCord9(convert1,    0, i2sOut,      1);
AudioControlSGTL5000        sgtl5000_1;

uint16_t count17, count27;
float level = 0.05f;

void setup(void) {
  AudioMemory(50);
  AudioMemory_F32(100);
  Serial.begin(300);  delay(1000);
  Serial.println("*** Test WDRC2 Gain Compressor  **");
  sine1.frequency(1000.0f);
  sine1.amplitude(0.05f);
  // CAUTION - If using ears on the output, adjust the following two carefully
  gain0.setGain_dB(-25.0f);  // Consider (-50.0f);
  gain1.setGain_dB(13.0f);   // Consider (-30.0f);
  sgtl5000_1.enable();
  // Fir Filter needs coefs, now it ts just a pass through.
  count17 = 0;
  count27 = 0;

#if 0
  // For reference, here are the defaults from AudioEffectsWDRC_F32.h
  int16_t delaySize = 256;     // Any power of 2, i.e., 256, 128, 64, etc.
  float   gain0DB = 38.0f;     // Gain, in dB for low level inputs (dependent variable)
  float   gainOffsetDB = 0.0f; // Raise/lower entire gain curve by this amount (post gain)
  float   knee1DB = -50.0f;    // First knee on the gain curve
  float   cr1 = 3.0f;          // Compression ratio on dB curve between knee1dB and knee2dB
  float   knee2DB = -20.0f;    // Second knee on the gain curve
  float   cr2 = 10.0f;         // Compression ratio on dB curve above knee2dB
#endif
  // Edit the following to change settings
  // Note: gain0 is a dependent variable, and not available as an input
  compressor1.setDelayBufferSize(128);
  compressor1.setOutputGainOffsetDB(0.0f);
  compressor1.setKnee1LowDB(-50.0f);
  compressor1.setCompressionRatioMiddleDB(3.0f);
  compressor1.setKnee2HighDB(-20.0f);
  compressor1.setCompressionRatioHighDB(10.0f);
}

void loop(void)
   {
   float32_t* pBuff;
   static uint16_t kk;

#if SIGNAL_SOURCE == CW
   // Literally Continuous Wave. Edit frequency and amplitude below
   pBuff = queue0.getBuffer();
   if (pBuff)
      {
      if(count27++ == 227)  // about 0.7 sec
         {
         sine1.frequency(1000.0f);  // <--
         sine1.amplitude(0.01f);    // <--
         Serial.print("  LowLevDB= ");  Serial.print( compressor1.getLowLevelGainDB(), 3);
         Serial.print("  CurInDB= ");  Serial.print( compressor1.getCurrentInputDB(), 3);
         Serial.print("  CurrentGainDB= ");  Serial.println( compressor1.getCurrentGainDB(), 3);
         count27 = 0;
         }
         // Multiply by 1.0 by filling queue1
         for(int ii=0; ii<128; ii++)
            *(pBuff + ii) = 1.0f;     // Fill buffer with constant level
         queue0.playBuffer();          // Starr up new 128 values
      }

#elif SIGNAL_SOURCE == RANDOM
   /* To give an audio signal with interest, we alter the frequency
    * every 17 blocks (49 msec) and alter the level every 27 b;ocks
    * (78.4 msec)  The pattern keeps changing to be more interesting
    * Janet thinks it is aliens. */
   pBuff = queue0.getBuffer();
   if (pBuff)
      {
      Serial.print("  CurInDB= ");  Serial.print( compressor1.getCurrentInputDB(), 3);
      Serial.print("  CurrentGainDB= ");  Serial.println( compressor1.getCurrentGainDB(), 3);
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
         }
      for(int ii=0; ii<128; ii++)
         *(pBuff + ii) = level;     // Fill buffer with constant level
      queue0.playBuffer();          // Starr up new 128 values
      }

#elif SIGNAL_SOURCE == POWER_SWEEP
   pBuff = queue0.getBuffer();
   if (pBuff)
      {
      if(count17++ == 17)
         {
         count17 = 0;
         level *= 1.05f;
         if(level > 0.99)
            {
            level=0.001;
            delay(200);
            }
         Serial.print("  CurInDB= ");  Serial.print( compressor1.getCurrentInputDB(), 3);
         Serial.print("  CurrentGainDB= ");  Serial.println( compressor1.getCurrentGainDB(), 3);
         }
      for(int ii=0; ii<128; ii++)
         *(pBuff + ii) = level;
      queue0.playBuffer();
      }
      
#elif SIGNAL_SOURCE == PULSE
   pBuff = queue0.getBuffer();
   if (pBuff)
      {
      for(int ii=0; ii<128; ii++)
         *(pBuff + ii) = 1.0f;
      queue0.playBuffer();
      // A pulse, repeats every 3 minutes or so
      if(count17 == 5)     sine1.amplitude(0.0f);  // Settling
      else if(count17 == 498)   compressor1.printOn(true); //record it
      else if(count17 == 500)   sine1.amplitude(0.03f);
      else if(count17 == 510)   sine1.amplitude(0.0f);
      else if(count17 == 700)   compressor1.printOn(false);
      // or build your own transient test pulse here
      count17++;
      } 
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
