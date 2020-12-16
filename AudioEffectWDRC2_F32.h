/*
 * AudioEffectCompWDR2_F32: Wide Dynamic Rnage Compressor #2
 *
 * Bob Larkin  W7PUA  11 December 2020    ***********  UNDER DEVELOPMENT SUBJECT TO CHANGE!!!!
 * This is an attempt to simplify and further comment the Chip Audette WDRC compressor.
 * Derived from: Chip Audette (OpenAudio) Feb 2017
 * Which was derived From: WDRC_circuit from CHAPRO from BTNRC: https://github.com/BTNRH/chapro
 *     As of Feb 2017, CHAPRO license is listed as "Creative Commons?"
 *
 * MIT License.  Use at your own risk.
 */
/*
 * WDRC2  Wide dynamic range compressor #2.  Amplifies input signals by a fixed amoount
 * when the input is low.  Above a first knee, the gain is reduce progressively more as
 * the input gets greater.  On a dB out vs. dB in curve, this shows as a chnge in the
 * original 1:1 slope to a lesser slope of 1:cr1 where cr1 is the first compression ratio.
 * Finally there is a second knee where the gain is reduced at an even greater rate.  In the
 * extreme this becomes a hard limiter, but it can continue to increase slightly at a dB
 * rate of 1:cr2, with cr2 the second compression ratio.

    Vout dB
      |
      |
   0.0|                                               **********#
      |                                       **********
      |                            @**********  1:cr2
      |                        ****
      |                     ***
      |                  ***
      |               *** 1:cr1
      |            ***
      |        @***
      |       *
      |      *
      |     *
      |    * Vout = Vin + g0 (all in dB)
      |   *   1:1
      |  *
      | *                            * Vout vs. Vin in dB *
      |*                        Knees (breakpoints) are shown with '@'
      *                         Zero, zero intersection shown with '#'
     *|                         Slopes are ratio of:  output:input  (in dB)
    * |
   *  |________|___________________|____________________________|_________ Vin dB
               k1                  k2                          0.0

 * The graph shows the changes in gain on a log or dB scale.  A 1:1 slope represents
 * a constant gain with level.  When the slope is less, say cr1:1 where cr1 might be 3,
 * the voltage gain is decreasing as the input level increases.
 *
 * The model here is, I believe, the same as the two references above (Audette and CHAPRO).
 * The variable names have been changed to avoid confusion with those of audiologists and
 * to be easier to follow for non-audiologists.  Here goes:
 *   gain0DB  Gain, in dB of the compressor for low level inputs (g0 on graph)  [38 dB]
 *   knee1dB  First knee on the gain curve where the dB gain slope decreases(k1) [-50 dB]
 *   cr1      Compression ratio on dB curve between knee1dB and knee2dB   [3.0]
 *   knee2dB  Second knee on the gain curve where the dB gain slope decreases further (k2) [-20 dB]
 *   cr2      Compression ratio on dB curve above knee2dB  [10.0]
 *
 * The presets for the above quantities, shown in square brackest, are qite aggressive,
 * with a lot of compression (up to 38 dB).  This is for demonstration, and each
 *  situation will have different settings.  For the presets, the following data
 *  was measured, essentiallly as predicted:
 *    vIn (rel full scale)=0.001  vInDB=-60.05  vOutDB-vInDB=38.00
 *    vIn (rel full scale)=0.003  vInDB=-50.47  vOutDB-vInDB=38.00
 *    vIn (rel full scale)=0.01   vInDB=-40.00  vOutDB-vInDB=31.38
 *    vIn (rel full scale)=0.03   vInDB=-30.45  vOutDB-vInDB=24.97
 *    vIn (rel full scale)=0.1    vInDB=-19.98  vOutDB-vInDB=19.98
 *    vIn (rel full scale)=0.3    vInDB=-10.45  vOutDB-vInDB= 9.40
 *    vIn (rel full scale)=1.0    vInDB=  0.01  vOutDB-vInDB=-0.01
 * 
 * vInDB refers to the time averaged envelope voltage.
 * Needing a zero reference, this has been chosen as full ADC range output. This is ±1.0
 * peak or 0.707 RMS in F32 terminology.  If this is fixed, the low-level gain will also be
 * determined.  This is calculated in the constructor.
 *
 * The curve is for gainOffsetDB = 0.0. This parameter raises and lowers the entire gain
 * curve by this many dB.  This is equivalent to a post-compressor gain (or loss).
 *
 * Note: This is all done in conventional 10 based dB.  This ends up with scaling in
 * several places that could be eliminated by using 2B instead of dB, i.e.,
 * use log2() and 2^().  This would seem to be faster, but less "readable."
 * 
 *    ***********     UNDER DEVELOPMENT SUBJECT TO CHANGE!!!!      *********
 */

#ifndef _AudioEffectCompWDRC2_F32
#define _AudioEffectCompWDRC2_F32

#include <Arduino.h>
#include <AudioStream_F32.h>

class AudioEffectWDRC2_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName: CompressWDRC2
  public:
    AudioEffectWDRC2_F32(void): AudioStream_F32(1,inputQueueArray) {
    setAttackReleaseSec(0.005f, 0.100f);
    setLowLevelGain();   // Not an independent variable, set by knees, cr's and 0,0 intersection
    // setSampleRate_Hz(AUDIO_SAMPLE_RATE);
    }

    //AudioEffectCompWDRC_F32(AudioSettings_F32 settings): AudioStream_F32(1,inputQueueArray) {
    //  setSampleRate_Hz(settings.sample_rate_Hz);
    //}

    // Here is the method called automatically by the audio library
    void update(void) {
      float  vAbs, vPeak;
      float vInDB, vOutDB;
      float targetGain;

      // Receive the input audio data
      audio_block_f32_t *block = AudioStream_F32::receiveWritable_f32();
      if (!block) return;
      // Allocate memory for the output
      audio_block_f32_t *out_block = AudioStream_F32::allocate_f32();
      if (!out_block)
         {
          release(block);
          return;
          }

      // Find the smoothed envelope, target gain and compressed output
      vPeak = vPeakSave;
      for (int k=0; k<block->length; k++) {
          vAbs = (block->data[k] >= 0.0f) ? block->data[k] : -block->data[k];
          if (vAbs >= vPeak) {     // Attack (rising level)
              vPeak = alpha * vPeak + (oneMinusAlpha) * vAbs;
          } else {                 // Release (decay for falling level)
              vPeak = beta * vPeak;
          }
          // Convert to dB
          // At all levels and quite frequency flat, this under estimates by 1.05 dB
          vInDB = v2DB_Approx(vPeak) + 1.05f;
          // Convert to desired Vout_DB, this is the compression curve
          if(vInDB<=knee1DB)
              vOutDB = vInDB + gain0DB;          // No compression
          else if(vInDB<knee2DB)
              vOutDB = vInDB + gain0DB + (knee1DB - vInDB)*(cr1 - 1.0f)/cr1;  // Middle region
          else
              vOutDB = vInDB + gain0DB + (knee2DB - vInDB)*(cr2 - 1.0f)/cr2 +
                  (knee1DB - knee2DB)*(cr1 - 1)/cr1;    // High level region
          // A note: from the latter, algebra says for a 0, 0 intersection of vInDB and vOutDB
          // See setLowLevelGain()

          // Convert the needed gain back to a voltage ratio 10^(db/20)
          targetGain = pow10f(0.05f*(vOutDB - vInDB + gainOffsetDB));
          // And apply target gain to signal stream from the delayed data. The
          // delay buffer is circular because of delayBufferMask and length 2^m m<=8.
          out_block->data[k] = targetGain * delayData[(k + in_index) & delayBufferMask];

          if(printIO) {
             Serial.print(block->data[k],6);
             Serial.print("," );
             Serial.print(delayData[(k + in_index) & delayBufferMask],6);
             Serial.print("," );
             Serial.println(targetGain);
             }

          // Put the new data into the delay line, delaySize positions ahead.
          // If delaySize==256, this will be the same location as we just got data from.
          delayData[(k + in_index + delaySize) & delayBufferMask] = block->data[k];
        }
      vPeakSave = vPeak;            // save last vPeak for next time
      sampleInputDB = vInDB;        // Last values for get...() functions
      sampleGainDB = vOutDB - vInDB;
      // transmit the block and release memory
      AudioStream_F32::release(block);
      AudioStream_F32::transmit(out_block); // send the FIR output
      AudioStream_F32::release(out_block);
      // Update pointer in_index to delay line for next 128 update
      in_index = (in_index + block->length) & delayBufferMask;
    }

    // gain0DB is the gain at low levels, below compression.  Not an independent variable,
    // so this should becalled after any change is made to knees and compression ratios.
    void setLowLevelGain(void)
        {
        gain0DB = knee2DB*(1.0f - cr2)/cr2 + (knee2DB - knee1DB)*(cr1 - 1.0f)/cr1;   // Low-level gain
        }

    void setOutputGainOffsetDB(float _gOff) { gainOffsetDB = _gOff; }
    void setKnee1LowDB(float _k1) { knee1DB = _k1; }
    void setCompressionRatioMiddleDB(float _cr1) { cr1 = _cr1; }
    void setKnee2HighDB(float _k2) { knee2DB = _k2; }
    void setCompressionRatioHighDB(float _cr2) { cr2 = _cr2; }
    // A delay of 256 samples is 256/44100 = 0.0058 sec = 5.8 mSec
    void setDelayBufferSize(int16_t _delaySize) { // Any power of 2, i.e., 256, 128, 64, etc.
       delaySize = _delaySize;
       delayBufferMask = _delaySize - 1;
       in_index = 0;
       }
    void printOn(bool _printIO) { printIO = _printIO; } // Diagnostics ONLY. Not for general INO

    float getLowLevelGainDB(void) { return gain0DB; }
    float getCurrentInputDB(void) { return sampleInputDB; }
    float getCurrentGainDB(void)  { return sampleGainDB; }

    //convert time constants from seconds to unitless parameters, from CHAPRO, agc_prepare.c
    void setAttackReleaseSec(const float atk_sec, const float rel_sec) {
        // convert ANSI attack & release times to filter time constants
        float ansi_atk = atk_sec * sample_rate_Hz / 2.425f;
        float ansi_rel = rel_sec * sample_rate_Hz / 1.782f;
        alpha = (float) (ansi_atk / (1.0f + ansi_atk));
        oneMinusAlpha = 1.0f - alpha;
        beta = (float) (ansi_rel / (1.0f + ansi_rel));
    }

    // Accelerate the powf(10.0,x) function (from Chip's single slope compressor)
    float pow10f(float x) {
      //return powf(10.0f,x)       //standard, but slower
      return expf(2.30258509f*x);  //faster:  exp(log(10.0f)*x)
    }

    /* See https://github.com/Tympan/Tympan_Library/blob/master/src/AudioCalcGainWDRC_F32.h
     * Dr Paul Beckmann
     * https://community.arm.com/tools/f/discussions/4292/cmsis-dsp-new-functionality-proposal/22621#22621
     * Fast approximation to the log2() function.  It uses a two step
     * process.  First, it decomposes the floating-point number into
     * a fractional component F and an exponent E.  The fraction component
     * is used in a polynomial approximation and then the exponent added
     * to the result.  A 3rd order polynomial is used and the result
     * when computing db20() is accurate to 7.984884e-003 dB.  Y is log2(X)
     */
     float v2DB_Approx(float volts) {
     float Y, F;
     int E;
     // This is the approximation to log2()
     F = frexpf(volts, &E);    //   first separate power of 2;
     //  Y = C[0]*F*F*F + C[1]*F*F + C[2]*F + C[3] + E;
     Y = 1.23149591;   //C[0]
     Y *= F;
     Y += -4.11852516f; //C[1]
     Y *= F;
     Y += 6.02197014f;  //C[2]
     Y *= F;
     Y += -3.13396450f; //C[3]
     Y += E;
     // Convert to dB = 20 Log10(volts)
     return 6.020599f * Y;   // (20.0f/log2(10.0))*Y;
     }

  private:
    audio_block_f32_t *inputQueueArray[1];

    float delayData[256];   // The circular delay line for the signal
    uint16_t in_index = 0;      // Pointer to next block update entry
    // And a mask to make the circular buffer limit to a power of 2
    uint16_t delayBufferMask = 0X00FF;
    uint16_t delaySize = 256;

    float sample_rate_Hz = 44100;
    float attackSec = 0.005f;  // Q: Can this be reduced with the delay line added to the signal path??
    float releaseSec = 0.100f;
    // This alpha, beta for 5 ms attack, 100ms release, about 0.07 dB max ripple at 1000 Hz
    float alpha = 0.98912216f;
    float oneMinusAlpha = 0.01087784f;
    float beta = 0.9995961f;
    // Presets here should be studied/experimented with for each application
    float gain0DB = 38.0f;     // Gain, in dB for low level inputs
    float gainOffsetDB = 0.0f; // Raise/lower entire gain curve by this amount (post gain)
    float knee1DB = -50.0f;    // First knee on the gain curve
    float cr1 = 3.0f;          // Compression ratio on dB curve between knee1dB and knee2dB
    float knee2DB = -20.0f;    // Second knee on the gain curve
    float cr2 = 10.0f;         // Compression ratio on dB curve above knee2dB
    float vPeakSave = 0.0f;
    bool printIO = false;      // Diagnostics Only
    float sampleInputDB, sampleGainDB;
};
#endif
