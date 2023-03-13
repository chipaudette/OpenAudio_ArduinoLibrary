/*
 *     radioVoiceClipper_F32.h
 *
 * 12 March 2023 (c) copyright Bob Larkin
 * But with With much credit to:
 * Chip Audette (OpenAudio)
 * and of course, to PJRC for the Teensy and Teensy Audio Library
 *
 * The development of this Voice Clipper was by Bob Larkin, W7PUA, based
 * entirely on ideas and suggestions from Dave Hershberger, W9GR.
 * Many thanks to Dave.  Note that  this clipper is is a "real variable"
 * version of the Single Sideband CESSB clipper.  See the companion 
 * radioCESSBtransmit_F32.h class which uses all the same principles.
 * 
 * The input signal is a voice  (or tones) that will, in general, have
 * been compressed in amplitude, keeping the maximum amplitude close to
 * 1.0 peak-to-center.   For this class, clipping occurs for any input 
 * greater than 1/gainIn where gainIn comes from the public function
 * setGains().  Normally gainIn has a value around 1.5 and so clipping occurs
 * for inputs above peak levels of 2/3=0.667.  For this level of gaiIn,
 * there will be about 3 dB of increase in the average power of the voice
 * but still minimal perception of "over-processing."
 * 
 * Internally the audio is clipped at the higher levels and the resulting
 * out-of-band distion is low pass filtered.  Next, the overshoot that
 * occurs with the filter is removed by measuring the overshoot, low-pass
 * filtering the overshoot and subtracting it off.  All this requires
 * care with the timing as all of the filtering steps involve delays.
 * 
 * The compressor2 class in this F32 library is intended to precede this
 * class.
 * 
 * NOTE:  Do NOT follow this block with any non-linear phase filtering,
 * such as IIR.  Minimize any linear-phase filtering such as FIR.
 * Such activities enhance the overshoots and defeat the purpose of clipping.
* 
 * An important note:  This clipper is suitable for voice modes, such as
 * AM or NBFM.  Do not use this clipper ahead of a single sideband
 * transmitter.  That is what the CESSB class is for.
 *
 * The following reference has information on CESSB, in detail, as well
 * as on the use of clippers, similar to this one, in broadcast work:
 *   Hershberger, D.L. (2014): Controlled Envelope Single Sideband. QEX
 *   November/December 2014 pp3-13.
 *   http://www.arrl.org/files/file/QEX_Next_Issue/2014/Nov-Dec_2014/Hershberger_QEX_11_14.pdf
 *
 * Status:  Experimental
 *
 * Inputs:  0 is voice audio input
 * Outputs: 0 is clipped voice.
 *
 * Functions, available during operation:
 *   void setSampleRate_Hz(float32_t fs_Hz)  Allows dynamic sample rate change.
 *
 *   struct levels* getLevels(int what) {
 *		what = 0 returns a pointer to struct levels before data is ready
 *      what = 1 returns a pointer to struct levels
 *
 *   uint32_t levelDataCount() return countPower0
 *
 *   void setGains(float32_t gIn, float32_t gCompensate, float32_t gOut)
 *
 * Time: T3.6 For an update of a 128 sample block, estimated     microseconds
 *       T4.0 For an update of a 128 sample block, measured     microseconds
 *       These times are for a 48 ksps rate.
 *
 * NOTE:  Do NOT follow this block with any non-linear phase filtering,
 * such as IIR.  Minimize any linear-phase filtering such as FIR.
 * Such activities enhance the overshoots and defeat the purpose of clipping.
 */

#ifndef _radioVoiceClipper_f32_h
#define _radioVoiceClipper_f32_h

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"
#include "mathDSP_F32.h"

#define VC_SAMPLE_RATE_0      0
#define VC_SAMPLE_RATE_11_12  1
#define VC_SAMPLE_RATE_44_50  2
#define VC_SAMPLE_RATE_88_100 3

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
struct levelClipper {
    float32_t pwr0;
    float32_t peak0;
    float32_t pwr1;
    float32_t peak1;
    uint32_t countP;  // Number of averaged samples for pwr0.
    };

class radioVoiceClipper_F32 : public AudioStream_F32  {
//GUI: inputs:1, outputs:2 //this line used for automatic generation of GUI node
//GUI: shortName:CESSBTransmit  //this line used for automatic generation of GUI node
public:
    radioVoiceClipper_F32(void) :
       AudioStream_F32(1, inputQueueArray_f32)
         {
		 setSampleRate_Hz(AUDIO_SAMPLE_RATE);
		 //uses default AUDIO_SAMPLE_RATE from AudioStream.h
		 //setBlockLength(128);   Always default 128
	     }

    radioVoiceClipper_F32(const AudioSettings_F32 &settings) :
       AudioStream_F32(1, inputQueueArray_f32)
         {
	     setSampleRate_Hz(settings.sample_rate_Hz);
	     //setBlockLength(128);   Always default 128
         }

    // Sample rate starts at default 44.1 ksps. That will work.  Filters
    // are designed for 48 and 96 ksps, however.  This is a *required*
    // function at setup().
    void setSampleRate_Hz(const float _fs_Hz) {
        sample_rate_Hz = _fs_Hz;
        if(sample_rate_Hz>10900.0f && sample_rate_Hz<12600.0f)
            {
			// Design point is 12 ksps.  No initial decimation. Interpolate
            // to 24 ksps for clipping and then decimate back to 12 at the end.
			sampleRate = VC_SAMPLE_RATE_11_12;
            nW = 128;
            nC = 256;
            countLevelMax = 10;  // About 0.1 sec for 12 ksps 
            inverseMaxCount = 1.0f/(float32_t)countLevelMax;
            arm_fir_init_f32(&firInstInterpolate1I, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate1I[0], nC);
            arm_fir_init_f32(&firInstClipperI, 123, (float32_t*)clipperOut,
                   &pStateClipperI[0], nC);
            arm_fir_init_f32(&firInstOShootI, 123, (float32_t*)clipperOut,
                   &pStateOShootI[0], nC);
		    }
        else if(sample_rate_Hz>43900.0f && sample_rate_Hz<50100.0f)
            {
			// Design point is 48 ksps
			sampleRate = VC_SAMPLE_RATE_44_50;
            nW = 32;
            nC = 64;
            countLevelMax = 37;  // About 0.1 sec for 48 ksps
            inverseMaxCount = 1.0f/(float32_t)countLevelMax;
            arm_fir_decimate_init_f32(&decimateInst, 65, 4,
                   (float32_t*)decimateFilter48, &pStateDecimate[0], 128);
            arm_fir_init_f32(&firInstInterpolate1I, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate1I[0], nC);
            arm_fir_init_f32(&firInstClipperI, 123, (float32_t*)clipperOut,
                   &pStateClipperI[0], nC);
            arm_fir_init_f32(&firInstOShootI, 123, (float32_t*)clipperOut,
                   &pStateOShootI[0], nC);
            arm_fir_init_f32(&firInstInterpolate2I, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate2I[0], nC);
		    }
		else if(sample_rate_Hz>88000.0f && sample_rate_Hz<100100.0f)
            {
			// GET THINGS WORKING AT VC_SAMPLE_RATE_44_50 FIRST AND THEN FIX UP 96 ksps
			// Design point is 96 ksps
/*			sampleRate = VC_SAMPLE_RATE_88_100;     //<<<<<<<<<<<<<<<<<<<<<<FIXUP
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
			sampleRate = VC_SAMPLE_RATE_0;
            nW = 1;
            nC = 1;
		    }
        newLevelDataReady = false;
        }

    struct levelClipper* getLevels(int what) {
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

    virtual void update(void);

private:
    void sincos_Z_(float32_t ph);
    struct levelClipper levelData;
    audio_block_f32_t *inputQueueArray_f32[1];
    uint32_t jjj = 0;   // Used for diagnostic printing

    // Input/Output is at 12, 48 or 96 ksps.
    // Clipping and overshoot processing is at 24 ksps.
    // Next line is to indicate that setSampleRateHz() has not executed
    int sampleRate = VC_SAMPLE_RATE_0;
    float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;   // 44.1 ksps
    int16_t nW = 32;  // 128, 32 or 16
    int16_t nC = 64;  // 256, 64 or 32
    uint16_t  block_length = 128;

    float32_t pStateDecimate[128 + 65 - 1];    // Goes with CMSIS decimate function
    arm_fir_decimate_instance_f32 decimateInst;

    // For 12 ksps case, 24 kHz clipper uses 256 points 
    float32_t pStateInterpolate1I[256 + 23 - 1];  // For interpolate 12 to 24 ksps
    arm_fir_instance_f32 firInstInterpolate1I;

    float32_t pStateClipperI[256 + 123 - 1];    // Goes with Clipper filter
    arm_fir_instance_f32 firInstClipperI;      // at 24 ksps

    float32_t pStateOShootI[256+123-1];
    arm_fir_instance_f32 firInstOShootI;

    float32_t pStateInterpolate2I[256 + 23 - 1];  // For interpolate 12 to 24 ksps
    arm_fir_instance_f32 firInstInterpolate2I;

    float32_t gainIn = 1.0f;
    float32_t gainCompensate = 1.4f;
    float32_t gainOut = 1.0f;  // Does not change Clipping, here for convenience to set out level

    // A tiny delay to allow negative time for the previous path
    float32_t osEnv[4];
    uint16_t indexOsEnv = 4;  // 0 to 3 by using a 2-bit mask

    // We need a delay for overshoot remove to account for the FIR
    // filter in the correction path. Some where around 128 taps works
    // but if we make the delay exactly 2^6=64 the delay line is simple
    // resulting in a FIR size of 2*64+1=129 taps.
    float32_t osDelayI[64];
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
