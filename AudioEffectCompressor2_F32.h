/*
 *        AudioEffectCompressor2_F32.h
 *
 * Bob Larkin  W7PUA  11 December 2020
 *
 * This is a general purpose audio compressor block (C++ class). It works by determining
 * the average input of the input signal, and based on a pre-determined curve,
 * changes the gain going through the block.
 * A good discussion is the Wikipedia page:
 *     https://en.wikipedia.org/wiki/Dynamic_range_compression
 * This compressor includes up to 5 dB/dB line segments allowing for most of the
 * features listed.  These include
 *   Multi segment compression curves, up to 5
 *   Limiting
 *   Approximation to "soft knees"
 *   Expansion for suppressing low-level artifacts
 *   Anticipation
 *   Scale offset for use such as hearing-aid audiology
 * This is derived from the WDRC compressor. Chip Audette (OpenAudio) Feb 2017
 * Which was derived From: WDRC_circuit from CHAPRO from BTNRC: https://github.com/BTNRH/chapro
 * As of Feb 2017, CHAPRO license is listed as "Creative Commons?"
 *
 * MIT License.  Use at your own risk.
 */
/* Compressor #2.  Amplifies input signals by varying amoounts depending
 * on the signal level.  This is controlled by up to 5 line segments specified
 * by a point at the highest input level for the line along with a compression ratio
 * (1/slope) for the line segment. This can provide limiting at the highest inputs
 * by setting compressionRatio[0]=1000.0 (i.e., some big value).  Expansion at the
 * lw levels provides a squelch action, using a very small compression
 * ratio like 0.01.
 *
 * A special case is the [0] segment, that continues at the same slope for inputs up
 * to any level.  For this the kneeDB[0] can be the expected 0.0  or other values
 * on that line.  The output level for kneeDB[0] is the input variable, marginDB.
 * This allows gain control near clipping but below.  marginDB is typically 1 or 2 dB.
 *
    Vout dB
      |
      |
  0.0 +                                     kneeDB[0]
 marg +                        kneeDB[1]        @********************
      |                            @************    1:compressionRatio[0]
      |                        ****
      |                     ***
      |                  ***
      |               *** 1:compressionRatio[1]
      | kneeDB[2]  ***
      |        @***
      |       *
      |      *
      |     *
      |    *
      |   * 1:compressionRatio[2]
      |  *
      | *                         ===  Vout in dB vs. Vin in dB  ===
      |*                               Three segment example
      *                          Knees (breakpoints) are shown with '@'
     *|    compressionRatio[] are ratio of: input change (in dB):output change in dB
    * |
   *  |________|___________________|____________________________|________ vIn dB
               k1                  k2                          0.0

 * The graph shows the changes in gain on a log or dB scale.  A compressionRatio
 * of 1 represents a constant gain with level.  When the compressionRatio is greater
 * than 1, say 2.0, the voltage gain is decreasing as the input level increases.
 *
 * vInDB refers to the time averaged envelope voltage.
 * The zero reference is the full ADC range output. This is ±1.0
 * peak or 0.707 RMS in F32 terminology.
 *
 * The curve is for gainOffsetDB = 0.0. This parameter raises and lowers the
 * input scale for the kneeDB[] parameter.
 *
 * Timing: For 44.1 kHz sample rate and 256 samples per update, the update( ) time
 * runs 240 to 270 icroseconds using Teensy 3.6.
 */

#ifndef _AUDIO_EFFECT_COMPRESSOR2_F32_H
#define _AUDIO_EFFECT_COMPRESSOR2_F32_H

#include <Arduino.h>
#include <AudioStream_F32.h>

// The following 3 defines are simplified implementations for common uses.
// They replace the begin() function that is otherwise required.
// See testCompressor2.ino example for how to use these defines.
// None of these support offsetting the input scale as done in Tympan.

/* limiterBegin(pointerObject, float marDB, float linearInDB) has 2 segments.
 * It is linear up to an input of linearInDB (typically -15.0f) and
 * then virtually limits for higher input levels.  The output level at
 * this point is marDB, the margin to prevent clipping, like -2 dB.
 * This is not a clipper with waveform distortion, but rather decreases
 * the gain, dB for dB, as the input increases in the limiter region.
 * pobject is a pointer to the INO AudioEffectCompressor2_F32 object.
 * This function replaces begin() for the AudioEffectCompressor2_F32 object.
 */
#define limiterBegin(pobject, marDB, linearInDB)  struct compressionCurve _curv={marDB,0.0f,{0.0,linearInDB,-1000.0f,-1000.0f,-1000.0f},{100.0,1.0f,1.0f,1.0f,1.0f}}; pobject->setCompressionCurve(&_curv); pobject->begin();

/* basicCompressorBegin has a 3 segments.  It is linear up to an input linearInDB
 * and then decreases gain according to compressionRatioDB up to an input -10 dB where it
 * is almost limited, with an increase of output level of 1 dB for a 10 dB increase
 * in input level.  The output level at full input is 1 dB below full output.
 * This function replaces begin() for the AudioEffectCompressor2_F32 object.
 */
#define basicCompressorBegin(pobject, linearInDB, compressionRatio)  struct compressionCurve _curv={-1.0,0.0f,{0.0,-10.0f,linearInDB,-1000.0f,-1000.0f},{10.0f,compressionRatio,1.0f,1.0f,1.0f}}; pobject->setCompressionCurve(&_curv); pobject->begin();

/* squelchCompressorBegin is similar to basicCompression above, except that there is
 * an expansion region for low levels.  So, the call defines the four regions in
 * terms of the input levels.  squelchInDB sets the lowest input level
 * before the squelching effect starts. */
#define squelchCompressorBegin(pobject, squelchInDB, linearInDB, compressionInDB, compressionRatio) struct compressionCurve _curv={-1.0,0.0f,{0.0,compressionInDB,linearInDB,squelchInDB,-1000.0f},{10.0,compressionRatio,1.0f,0.1f,1.0f}}; pobject->setCompressionCurve(&_curv); pobject->begin();

// Basic definition of compression curve:
struct compressionCurve {
   float marginDB;
   float offsetDB;
   float kneeDB[5];
   float compressionRatio[5];
};

// ---------------------------------------------------------------------------

class AudioEffectCompressor2_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName: Compressor2
  public:
    AudioEffectCompressor2_F32(void): AudioStream_F32(1, inputQueueArray) {
       setAttackReleaseSec(0.005f, 0.100f);
       }

    AudioEffectCompressor2_F32(const AudioSettings_F32 &settings): AudioStream_F32(1, inputQueueArray) {
       //setSampleRate_Hz(settings.sample_rate_Hz);
       setAttackReleaseSec(0.005f, 0.100f);
       }

    virtual void update(void);
    void begin(void);
    void setCompressionCurve(struct compressionCurve*);
    // A delay of 256 samples is 256/44100 = 0.0058 sec = 5.8 mSec
    void setDelayBufferSize(int16_t _delaySize) { // Any power of 2, i.e., 256, 128, 64, etc.
       delaySize = _delaySize;
       delayBufferMask = _delaySize - 1;
       in_index = 0;
       }
    void printOn(bool _printIO) { printIO = _printIO; } // Diagnostics ONLY. Not for general INO
    float getCurrentInputDB(void) { return sampleInputDB; }
    float getCurrentGainDB(void)  { return sampleGainDB; }
    float getvInMaxDB(void)  {
       float vRet = vInMaxDB;
       vInMaxDB = -1000.0f;   // Reset for next max measure
       return vRet;
       }
    //convert time constants from seconds to unitless parameters, from CHAPRO, agc_prepare.c
    void setAttackReleaseSec(const float atk_sec, const float rel_sec) {
        // convert ANSI attack & release times to filter time constants
        float ansi_atk = atk_sec * sample_rate_Hz / 2.425f;
        float ansi_rel = rel_sec * sample_rate_Hz / 1.782f;
        alpha = (float) (ansi_atk / (1.0f + ansi_atk));
        oneMinusAlpha = 1.0f - alpha;
        beta = (float) (ansi_rel / (1.0f + ansi_rel));
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

    /*          Definition of the compression curve.
     * Input and output are normally referenced to the full scale range (-1.0 to 1.0 in float).
     * The full scale point is called 0 dB.
     * There are 5 slopes, specified by the top input level for each line segment (kneeDB[])
     * and the Compression Ratio (1/slope) of the segment (compressionRatio[]).
     * These are numbered from the highest to the lowest allowing
     * the number of segments to be adjusted, i.e., there is always a segment 0
     * but there may not be a segment 3 and 4, for instance. This becomes very flexible,
     * as there can be, say, compression for top levels and expansion for very low levels.
     *
     * A special case is segment 0.  On a plot of output dB vs input db, an input
     * level of kneeDB[0] produces an output of marginDB. A typical marginDB might
     * be -1.0 or -2.0.  This margin allows controlling the gain without clipping in the DAC.
     *
     * This structure can have multiple curve structures like cCurve that can be used by
     *     cmpr1.setCompressionCurve(&cCurve);
     *     cmpr1.begin();
     *
     * Unused segments can have knees like kneeDB[4] at, say -1000.0f.
     * Values below -500 will slightly speed up the update() function.  if the bottom
     * two segments are not used, kneeDB[3] would also be set to -1000.0f amd so forth.
     *
     * Finally, there is a variable offsetDB that allows for a shift in the input scale.
     * It simply shifts the definition input levels, in dB.
     * This allows converting from DSP scales that have 0dB at full DSP scale
     * to auditory scales such as are used in the Tympan library. An offsetDB=119.0
     * would allow all inputs to be in "SPL" units with a maximum input value of 119.
     */
     struct compressionCurve curve0 = { -2.0f, 0.0f,        // margin, offset
        {0.0f, -10.0f, -20.0f, -30.0f, -1000.0f},           // kneeDB[]
        {  100.0f,  2.0f,   1.5f,     1.0f,      1.0f} };   // compressionRatio
    // VoutDB at each knee, needed to find gain; determined at begin():
    float outKneeDB[5]={-2.0f, -3.0f, -8.0f -978.0f, -978.0f};
    // slopes are 1/compressionRatio determined at begin() to save update{} time
    float slope[5] = {0.01f, 0.5f, 0.666667f, 1.0f, 1.0f};

    // Save time in update() by ignoring unused (low level) segments:
    uint16_t firstIndex = 3;
    float vPeakSave = 0.0f;
    float vInMaxDB = -1000.0f;    // Only for reporting
    bool printIO = false;      // Diagnostics Only
    float sampleInputDB, sampleGainDB;
};
#endif
