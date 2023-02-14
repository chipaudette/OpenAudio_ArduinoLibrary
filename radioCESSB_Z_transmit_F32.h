/*
 *     radioCESSB_Z_transmit_F32.h
 *
 * This is a modification of the CESSB algorithm to output SSB at zero carrier
 * instead of 1350 Hz as the Weaver modulation produces.  This allows
 * transmission with zero-IF radios where the finite carrier balance
 * of the hardware mixers produces the mid-band tone.The basic change
 * is to use the phasing method in place of the Weaver method.  However,
 * all filters needed to be changed and are at the bottom of this file.
 * The 12 and 24 ksps sample rates of the radioCESSB_transmit_F32 class
 * are continued here as they were more than adequate for the Weaver method.
 * The sine/cosine oscillator is not needed here and has been removed.
 *
 *
 * 18 Jan 2023 (c) copyright Bob Larkin
 * But with With much credit to:
 * Chip Audette (OpenAudio)
 * and of course, to PJRC for the Teensy and Teensy Audio Library
 *
 * The development of the Controlled Envelope Single Side Band (CESSB)
 * was done by Dave Hershberger, W9GR. Many thanks to Dave.
 * The following description is mostly taken
 * from Frank, DD4WH and is on line at the GNU Radio site, ref:
 * https://github-wiki-see.page/m/df8oe/UHSDR/wiki/Controlled-Envelope-Single-Sideband-CESSB
 * and has been revised by Bob L. to reflect the phasing method change.
 *
 * Controlled Envelope Single Sideband is an invention by Dave Hershberger
 * W9GR with the aim to "allow your rig to output more average power while
 * keeping peak envelope power PEP the same". The increase in perceived
 * loudness can be up to 4dB without any audible increase in distortion
 * and without making you sound "processed" (Hershberger 2014, 2016b).
 *
 * The principle to achieve this is relatively simple. The process
 * involves only audio baseband processing which can be done digitally in
 * software without the need for modifications in the hardware or messing
 * with the RF output of your rig.
 *
 * Controlled Envelope Single Sideband can be produced using three
 * processing blocks making up a complete CESSB system:
 *   1. An SSB modulator. This is implemented as the phasing method to allow
 *      minimum (12 kHz) decimated sample rate with the output of I & Q
 *      signals (a complex SSB signal).
 *   2. A baseband envelope clipper. This takes the modulus of the I & Q
 *      signals (also called the magnitude), which is sqrt(I * I + Q * Q)
 *      and divides the I & Q signals by the modulus, IF the signal is
 *      larger than 1.0. If not, the signal remains untouched. After
 *      clipping, the signal is lowpass filtered with a linear phase FIR
 *      low pass filter with a stopband frequency of 3.0kHz
 *   3. An overshoot controller . This does something similar as the
 *      envelope clipper: Again, the modulus is calculated (but now on
 *      the basis of the current and two preceding and two subsequent
 *      samples). If the signals modulus is larger than 1 (clipping),
 *      the signals I and Q are divided by the maximum of 1 or of
 *      (1.9 * signal). That means the clipping is overcompensated by 1.9
 *      [the phasing method seems to perform best with 1.4*signal]
 *      which leads to a much better suppression of the overshoots from
 *      the first two stages. Finally, the resulting signal is again
 *      lowpass-filtered with a linear phase FIR filter with stopband
 *      frequency of 3.0khz
 *
 * It is important that the sample rate is high enough so that the higher
 * frequency components of the output of the modulator, clipper and
 * overshoot controller do not alias back into the desired signal. Also
 * all the filters should be linear phase filters (FIR, not IIR).
 *
 * This CESSB system can reduce the overshoot of the SSB modulator from
 * 61% to 1.3%, meaning about 2.5 times higher perceived SSB output power
 * (Hershberger 2014).
 *
 * References:
 * 1-Hershberger, D.L. (2014): Controlled Envelope Single Sideband. QEX
 *   November/December 2014 pp3-13.
 *   http://www.arrl.org/files/file/QEX_Next_Issue/2014/Nov-Dec_2014/Hershberger_QEX_11_14.pdf
 * 2-Hershberger, D.L. (2016a): External Processing for Controlled
 *   Envelope Single Sideband. - QEX January/February 2016 pp9-12.
 *   http://www.arrl.org/files/file/QEX_Next_Issue/2016/January_February_2016/Hershberger_QEX_1_16.pdf
 * 3-Hershberger, D.L. (2016b): Understanding Controlled Envelope Single
 *   Sideband. - QST February 2016 pp30-36.
 * 4-Forum discussion on CESSB on the Flex-Radio forum,
 *   https://community.flexradio.com/discussion/6432965/cessb-questions
 *
 * Status:  Experimental
 *
 * Inputs:  0 is voice audio input
 * Outputs: 0 is I     1 is Q
 *
 * Functions, available during operation:
 *   void frequency(float32_t fr)  Sets LO frequency Hz
 *
 *   void setSampleRate_Hz(float32_t fs_Hz)  Allows dynamic sample rate change for this function
 *
 *   struct levels* getLevels(int what) {
 *		what = 0 returns a pointer to struct levels before data is ready
 *      what = 1 returns a pointer to struct levels
 *
 *   uint32_t levelDataCount() return countPower0
 *
 *  void setGains(float32_t gIn, float32_t gCompensate, float32_t gOut)
 *
 * Time: T3.6 For an update of a 128 sample block, estimated 700 microseconds
 *       T4.0 For an update of a 128 sample block, measured 211 microseconds
 *       These times are for a 48 ksps rate.
 *
 * NOTE:  Do NOT follow this block with any non-linear phase filtering,
 * such as IIR.  Minimize any linear-phase filtering such as FIR.
 * Such activities enhance the overshoots and defeat the purpose of CESSB.
 */

#ifndef _radioCESSB_Z_transmit_f32_h
#define _radioCESSB_Z_transmit_f32_h

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"
#include "mathDSP_F32.h"

#define SAMPLE_RATE_0      0
#define SAMPLE_RATE_44_50  1
#define SAMPLE_RATE_88_100 2

#ifndef M_PI
#define M_PI   3.141592653589793f
#endif

#ifndef M_PI_2
#define M_PI_2 1.570796326794897f
#endif

#ifndef M_TWOPI
#define M_TWOPI  (M_PI * 2.0f)
#endif

    // For the average power and peak voltage readings, global
struct levelsZ {
    float32_t pwr0;
    float32_t peak0;
    float32_t pwr1;
    float32_t peak1;
    uint32_t countP;  // Number of averaged samples for pwr0.
    };

class radioCESSB_Z_transmit_F32 : public AudioStream_F32  {
//GUI: inputs:1, outputs:2 //this line used for automatic generation of GUI node
//GUI: shortName:CESSBTransmit  //this line used for automatic generation of GUI node
public:
    radioCESSB_Z_transmit_F32(void) :
       AudioStream_F32(1, inputQueueArray_f32)
         {
		 setSampleRate_Hz(AUDIO_SAMPLE_RATE);
		 //uses default AUDIO_SAMPLE_RATE from AudioStream.h
		 //setBlockLength(128);   Always default 128
	     }

    radioCESSB_Z_transmit_F32(const AudioSettings_F32 &settings) :
       AudioStream_F32(1, inputQueueArray_f32)
         {
	     setSampleRate_Hz(settings.sample_rate_Hz);
	     //setBlockLength(128);   Always default 128
         }

    // Sample rate starts at default 44.1 ksps. That will work.  Filters
    // are designed for 48 and 96 ksps, however.  This is a *required*
    // function at setup().
    void setSampleRate_Hz(const float fs_Hz) {
        sample_rate_Hz = fs_Hz;
        if(sample_rate_Hz>44000.0f && sample_rate_Hz<50100.0f)
            {
			// Design point is 48 ksps
			sampleRate = SAMPLE_RATE_44_50;
            nW = 32;
            nC = 64;
            countLevelMax = 37;  // About 0.1 sec for 48 ksps
            inverseMaxCount = 1.0f/(float32_t)countLevelMax;

            arm_fir_decimate_init_f32(&decimateInst, 65, 4,
                   (float32_t*)decimateFilter48, &pStateDecimate[0], 128);

            arm_fir_init_f32(&firInstHilbertI, 201, (float32_t*)hilbert201_130Hz12000Hz,
                   &pStateHilbertI[0], nW);

            arm_fir_init_f32(&firInstInterpolate1I, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate1I[0], nC);
            arm_fir_init_f32(&firInstInterpolate1Q, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate1Q[0], nC);

            arm_fir_init_f32(&firInstClipperI, 123, (float32_t*)clipperOut,
                   &pStateClipperI[0], nC);
            arm_fir_init_f32(&firInstClipperQ, 123, (float32_t*)clipperOut,
                   &pStateClipperQ[0], nC);

            arm_fir_init_f32(&firInstOShootI, 123, (float32_t*)clipperOut,
                   &pStateOShootI[0], nC);
            arm_fir_init_f32(&firInstOShootQ, 123, (float32_t*)clipperOut,
                   &pStateOShootQ[0], nC);

            arm_fir_init_f32(&firInstInterpolate2I, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate2I[0], nC);
            arm_fir_init_f32(&firInstInterpolate2Q, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate2Q[0], nC);
		    }
		else if(sample_rate_Hz>88000.0f && sample_rate_Hz<100100.0f)
            {
			// GET THINGS WORKING AT SAMPLE_RATE_44_50 FIRST AND THEN FIX UP 96 ksps
			// Design point is 96 ksps
/*			sampleRate = SAMPLE_RATE_88_100;     //<<<<<<<<<<<<<<<<<<<<<<FIXUP
            nW = 16;
            nC = 32;
            countLevelMax = 75;  // About 0.1 sec for 96 ksps
            inverseMaxCount = 1.0f/(float32_t)countLevelMax;
            arm_fir_decimate_init_f32 (&decimateInst, 55, 4,
                   (float32_t*)decimateFilter48, pStateDecimate, 128);
            arm_fir_init_f32(&firInstClipper, 199, basebandFilter,
                   &StateFirClipperF32[0], 128);
 */
		    }
		else
            {
			// Unsupported sample rate
			sampleRate = SAMPLE_RATE_0;
            nW = 1;
            nC = 1;
		    }
        newLevelDataReady = false;
        }

    struct levelsZ* getLevels(int what) {
		if(what != 0)  // 0 leaves a way to get pointer before data is ready
 		    {
            levelData.pwr0 = powerSum0/((float32_t)countPower0);
            levelData.peak0 = maxMag0;
            levelData.pwr1 = powerSum1/(float32_t)countPower1;
            levelData.peak1 = maxMag1;
            levelData.countP = countPower0;

            // Automatic reset for next set of readings
            powerSum0 = 0.0f;
            maxMag0 = -1.0f;
            powerSum1 = 0.0f;
            maxMag1 = -1.0f;
            countPower0 = 0;
            countPower1 = 0;
		    }
        return &levelData;
	    }

    uint32_t levelDataCount(void) {
		return countPower0;     // Input count, out may be different
	    }

    void setGains(float32_t gIn, float32_t gCompensate, float32_t gOut)
        {
        gainIn = gIn;
        gainCompensate = gCompensate;
        gainOut = gOut;
	    }

    // Small corrections at the output end of this object can patch up hardware flaws.
    // _gI should be close to 1.0, _gXIQ and _gXQI should be close to 0.0.
    void setIQCorrections(bool _useCor, float32_t _gI, float32_t _gXIQ, float32_t _gXQI)
        {
        useIQCorrection = _useCor;
        gainI = _gI;
        crossIQ = _gXIQ;
        crossQI = _gXQI;
	    }

    // The LSB/USB selection depends on the processing of the IQ signals
    // inside this class.  It may get flipped with later processing.
    void setSideband(bool _sbReverse)
        {
        sidebandReverse = _sbReverse;
	    }

    virtual void update(void);

private:
    void sincos_Z_(float32_t ph);
    struct levelsZ levelData;
    audio_block_f32_t *inputQueueArray_f32[1];
    uint32_t jjj = 0;   // Used for diagnostic printing

    // Input/Output is at 48 or 96 ksps.  Hilbert generation is at 12 ksps.
    // Clipping and overshoot processing is at 24 ksps.
    // Next line is to indicate that setSampleRateHz() has not executed
    int sampleRate = SAMPLE_RATE_0;
    float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;   // 44.1 ksps
    int16_t nW = 32;  // 32 or 16
    int16_t nC = 64;  // 64 or 32
    uint16_t  block_length = 128;
    bool sidebandReverse = false;

    bool useIQCorrection = false;
    float32_t gainI = 1.0f;
    float32_t crossIQ = 0.0f;
    float32_t crossQI = 0.0f;

    float32_t pStateDecimate[128 + 65 - 1];    // Goes with CMSIS decimate function
    arm_fir_decimate_instance_f32 decimateInst;

    float32_t pStateHilbertI[32 + 201 - 1];
    arm_fir_instance_f32 firInstHilbertI;

    float32_t pStateInterpolate1I[64 + 23 - 1];  // For interpolate 12 to 24 ksps
    arm_fir_instance_f32 firInstInterpolate1I;
    float32_t pStateInterpolate1Q[64 + 23 - 1];
    arm_fir_instance_f32 firInstInterpolate1Q;


    float32_t pStateClipperI[64 + 123 - 1];    // Goes with Clipper filter
    arm_fir_instance_f32 firInstClipperI;      // at 24 ksps
    float32_t pStateClipperQ[64 + 123 - 1];
    arm_fir_instance_f32 firInstClipperQ;


    float32_t pStateOShootI[64+123-1];
    arm_fir_instance_f32 firInstOShootI;
    float32_t pStateOShootQ[64+123-1];
    arm_fir_instance_f32 firInstOShootQ;

    float32_t pStateInterpolate2I[128 + 23 - 1];  // For interpolate 12 to 24 ksps
    arm_fir_instance_f32 firInstInterpolate2I;
    float32_t pStateInterpolate2Q[128 + 23 - 1];
    arm_fir_instance_f32 firInstInterpolate2Q;

//    float32_t sn, cs;
    float32_t gainIn = 1.0f;
    float32_t gainCompensate = 1.4f;
    float32_t gainOut = 1.0f;  // Does not change CESSB, here for convenience to set transmit power

    float32_t delayHilbertQ[128];
    uint16_t indexDelayHilbertQ = 0;

    // A tiny delay to allow negative time for the previous path
    float32_t osEnv[4];
    uint16_t indexOsEnv = 4;  // 0 to 3 by using a 2-bit mask

    // We need a delay for overshoot remove to account for the FIR
    // filter in the correction path. Some where around 128 taps works
    // but if we make the delay exactly 2^6=64 the delay line is simple
    // resulting in a FIR size of 2*64+1=129 taps.
    float32_t osDelayI[64];
    float32_t osDelayQ[64];
    uint16_t indexOsDelay = 64;

    // RMS and Peak variable for monitoring levels and changes to the
    // Peak to RMS ratio.  These are temporary storage.  Data is
    // transferred by global levelData struct at the top of this file.
    float32_t powerSum0 = 0.0f;
    float32_t maxMag0 = -1.0f;
    float32_t powerSum1 = 0.0f;
    float32_t maxMag1 = -1.0f;
    uint32_t  countPower0 = 0;
    uint32_t  countPower1 = 0;

    bool newLevelDataReady = false;
    int countLevel = 0;
    int countLevelMax = 37;  // About 0.1 sec for 48 ksps
    float32_t inverseMaxCount = 1.0f/(float32_t)countLevelMax;

/* Input filter for decimate by 4:
 * FIR filter designed with http://t-filter.appspot.com
 * Sampling frequency: 48000 Hz
 * 0 Hz - 3000 Hz      ripple = 0.075 dB
 * 6000 Hz - 24000 Hz  atten = -95.93 dB  */
const float32_t decimateFilter48[65] = {
 0.00004685f, 0.00016629f, 0.00038974f, 0.00073279f, 0.00113663f, 0.00148721f,
 0.00159057f, 0.00125129f, 0.00032821f,-0.00114283f,-0.00289782f,-0.00441933f,
-0.00505118f,-0.00418143f,-0.00151748f, 0.00268876f, 0.00751487f, 0.01147689f,
 0.01286243f, 0.01027735f, 0.00323528f,-0.00737003f,-0.01913035f,-0.02842381f,
-0.03117447f,-0.02390063f,-0.00480378f, 0.02544011f, 0.06344286f, 0.10357132f,
 0.13904464f, 0.16342506f, 0.17210799f, 0.16342506f, 0.13904464f, 0.10357132f,
 0.06344286f, 0.02544011f,-0.00480378f,-0.02390063f,-0.03117447f,-0.02842381f,
-0.01913035f,-0.00737003f, 0.00323528f, 0.01027735f, 0.01286243f, 0.01147689f,
 0.00751487f, 0.00268876f,-0.00151748f,-0.00418143f,-0.00505118f,-0.00441933f,
-0.00289782f,-0.00114283f, 0.00032821f, 0.00125129f, 0.00159057f, 0.00148721f,
 0.00113663f, 0.00073279f, 0.00038974f, 0.00016629f, 0.00004685};

/* 90 degree Hilbert filter
 * FIR filter designed Iowa Hills suite - Thank you.
 * Sampling frequency: 12000 Hz
 * 130 Hz - 5870 Hz      ripple = 0.0036 dB  */
const float32_t hilbert201_130Hz12000Hz[201] = {
 0.000000000f, 0.000081360f, 0.000000000f, 0.000114966f, 0.000000000f, 0.000155734f,
 0.000000000f, 0.000204564f, 0.000000000f, 0.000262417f, 0.000000000f, 0.000330320f,
 0.000000000f, 0.000409359f, 0.000000000f, 0.000500689f, 0.000000000f, 0.000605532f,
 0.000000000f, 0.000725179f, 0.000000000f, 0.000860994f, 0.000000000f, 0.001014419f,
 0.000000000f, 0.001186978f, 0.000000000f, 0.001380282f, 0.000000000f, 0.001596041f,
 0.000000000f, 0.001836068f, 0.000000000f, 0.002102298f, 0.000000000f, 0.002396800f,
 0.000000000f, 0.002721798f, 0.000000000f, 0.003079696f, 0.000000000f, 0.003473107f,
 0.000000000f, 0.003904895f, 0.000000000f, 0.004378221f, 0.000000000f, 0.004896603f,
 0.000000000f, 0.005463995f, 0.000000000f, 0.006084876f, 0.000000000f, 0.006764381f,
 0.000000000f, 0.007508449f, 0.000000000f, 0.008324026f, 0.000000000f, 0.009219325f,
 0.000000000f, 0.010204165f, 0.000000000f, 0.011290428f, 0.000000000f, 0.012492662f,
 0.000000000f, 0.013828919f, 0.000000000f, 0.015321902f, 0.000000000f, 0.017000603f,
 0.000000000f, 0.018902655f, 0.000000000f, 0.021077827f, 0.000000000f, 0.023593325f,
 0.000000000f, 0.026542141f, 0.000000000f, 0.030056654f, 0.000000000f, 0.034331851f,
 0.000000000f, 0.039667098f, 0.000000000f, 0.046546491f, 0.000000000f, 0.055806835f,
 0.000000000f, 0.069029606f, 0.000000000f, 0.089604827f, 0.000000000f, 0.126348239f,
 0.000000000f, 0.211587134f, 0.000000000f, 0.636276105f, 0.000000000f,-0.636276105f,
 0.000000000f,-0.211587134f, 0.000000000f,-0.126348239f, 0.000000000f,-0.089604827f,
 0.000000000f,-0.069029606f, 0.000000000f,-0.055806835f, 0.000000000f,-0.046546491f,
 0.000000000f,-0.039667098f, 0.000000000f,-0.034331851f, 0.000000000f,-0.030056654f,
 0.000000000f,-0.026542141f, 0.000000000f,-0.023593325f, 0.000000000f,-0.021077827f,
 0.000000000f,-0.018902655f, 0.000000000f,-0.017000603f, 0.000000000f,-0.015321902f,
 0.000000000f,-0.013828919f, 0.000000000f,-0.012492662f, 0.000000000f,-0.011290428f,
 0.000000000f,-0.010204165f, 0.000000000f,-0.009219325f, 0.000000000f,-0.008324026f,
 0.000000000f,-0.007508449f, 0.000000000f,-0.006764381f, 0.000000000f,-0.006084876f,
 0.000000000f,-0.005463995f, 0.000000000f,-0.004896603f, 0.000000000f,-0.004378221f,
 0.000000000f,-0.003904895f, 0.000000000f,-0.003473107f, 0.000000000f,-0.003079696f,
 0.000000000f,-0.002721798f, 0.000000000f,-0.002396800f, 0.000000000f,-0.002102298f,
 0.000000000f,-0.001836068f, 0.000000000f,-0.001596041f, 0.000000000f,-0.001380282f,
 0.000000000f,-0.001186978f, 0.000000000f,-0.001014419f, 0.000000000f,-0.000860994f,
 0.000000000f,-0.000725179f, 0.000000000f,-0.000605532f, 0.000000000f,-0.000500689f,
 0.000000000f,-0.000409359f, 0.000000000f,-0.000330320f, 0.000000000f,-0.000262417f,
 0.000000000f,-0.000204564f, 0.000000000f,-0.000155734f, 0.000000000f,-0.000114966f,
 0.000000000f,-0.000081360f, 0.000000000};

/* Filter for outputs of clipper
 * Use also overshoot corrector, but might be able to use less terms.
 * FIR filter designed with http://t-filter.appspot.com
 * Sample frequency: 24000 Hz
 * 0 Hz - 2800 Hz      ripple = 0.14 dB
 * 3200 Hz - 12000 Hz  atten = 40.51 dB  */
const float32_t clipperOut[123] = {
-0.003947255f, 0.001759588f, 0.002221444f, 0.002407244f, 0.001833343f, 0.000524622f,
-0.000946260f,-0.001768428f,-0.001395297f, 0.000055916f, 0.001779024f, 0.002694998f,
 0.002099736f, 0.000157764f,-0.002092190f,-0.003282801f,-0.002542927f,-0.000116969f,
 0.002694319f, 0.004153363f, 0.003197589f, 0.000143560f,-0.003346600f,-0.005148200f,
-0.003947437f,-0.000152425f, 0.004166345f, 0.006378882f, 0.004871469f, 0.000164557f,
-0.005173898f,-0.007896395f,-0.006014470f,-0.000173552f, 0.006447615f, 0.009828080f,
 0.007480359f, 0.000184482f,-0.008116957f,-0.012379161f,-0.009436712f,-0.000194737f,
 0.010412610f, 0.015941971f, 0.012213107f, 0.000200845f,-0.013823966f,-0.021360759f,
-0.016552097f,-0.000205707f, 0.019544260f, 0.030836344f, 0.024523278f, 0.000211298f,
-0.031509151f,-0.052450055f,-0.044811840f,-0.000214078f, 0.074661107f, 0.158953216f,
 0.225159581f, 0.250214862f, 0.225159581f, 0.158953216f, 0.074661107f,-0.000214078f,
-0.044811840f,-0.052450055f,-0.031509151f, 0.000211298f, 0.024523278f, 0.030836344f,
 0.019544260f,-0.000205707f,-0.016552097f,-0.021360759f,-0.013823966f, 0.000200845f,
 0.012213107f, 0.015941971f, 0.010412610f,-0.000194737f,-0.009436712f,-0.012379161f,
-0.008116957f, 0.000184482f, 0.007480359f, 0.009828080f, 0.006447615f,-0.000173552f,
-0.006014470f,-0.007896395f,-0.005173898f, 0.000164557f, 0.004871469f, 0.006378882f,
 0.004166345f,-0.000152425f,-0.003947437f,-0.005148200f,-0.003346600f, 0.000143560f,
 0.003197589f, 0.004153363f, 0.002694319f,-0.000116969f,-0.002542927f,-0.003282801f,
-0.002092190f, 0.000157764f, 0.002099736f, 0.002694998f, 0.001779024f, 0.000055916f,
-0.001395297f,-0.001768428f,-0.000946260f, 0.000524622f, 0.001833343f, 0.002407244f,
 0.002221444f, 0.001759588f,-0.003947255f};

/* FIR filter designed with http://t-filter.appspot.com
 * Sampling frequency: 24000 sps
 * 0 Hz - 3000 Hz  gain = 2    ripple = 0.11 dB
 * 6000 Hz - 12000 Hz  atten = -62.4 dB
 * (At Sampling Frequency=48ksps, double all frequency values) */
const float32_t interpolateFilter1[23] = {
-0.00413402f,-0.01306124f,-0.01106321f, 0.01383359f, 0.04386756f, 0.02731837f,
-0.05470066f,-0.12407408f,-0.04389386f, 0.23355907f, 0.56707488f, 0.71763165f,
 0.56707488f, 0.23355907f,-0.04389386f,-0.12407408f,-0.05470066f, 0.02731837f,
 0.04386756f, 0.01383359f,-0.01106321f,-0.01306124f,-0.00413402};

};      // end Class
#endif
