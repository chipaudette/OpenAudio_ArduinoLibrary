/*
 *     radioCESSBtransmit_F32.h
 *
 * 2 Dec 2022     Bob Larkin
 * With much credit to:
 * Chip Audette (OpenAudio) Feb 2017
 * and of course, to PJRC for the Teensy and Teensy Audio Library
 *


 * Weaver Method of SSB:  Note that this class includes a good umplementation
 * of the Weaver method.  To use this without invoking the CESSB corrections,
 * just keep the input peak level below 1.0.  One could disable CESSB by
 * setting gainCompensate=0.0, but that serves no purpose if the input level
 * is below the clipping point.
 *
 * Status:  44 to 50 ksps sample rate working per ref 1 above.
 *          96 ksps is not yet implemented.  Anyone need this?
 *
 * Inputs:  0 is voice audio input
 * Outputs: 0 is I     1 is Q
 *
 * Functions, available during operation:
 *   void frequency(float32_t fr)  Sets LO frequency Hz
 *
 *   void setSampleRate_Hz(float32_t fs_Hz)  Allows dynamic sample rate
 *      change for this function
 *
 *   struct levels* getLevels(int what) {
 *		what = 0 returns a pointer to struct levels before data is ready
 *      what = 1 returns a pointer to struct levels
 *
 *   uint32_t levelDataCount() return countPower0
 *
 *  void setGains(float32_t gIn, float32_t gCompensate, float32_t gOut)
 *
 * Time: T3.6 For an update of a 128 sample block, estimated 750 microseconds
 *       T4.0 For an update of a 128 sample block, measured 252 microseconds
 *       These times are for a 48 ksps rate, for which about 2667 microseconds
 *       are available.
 */

#ifndef _radioCESSBtransmit_f32_h
#define _radioCESSBtransmit_f32_h

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
struct levels {
    float32_t pwr0;
    float32_t peak0;
    float32_t pwr1;
    float32_t peak1;
    uint32_t countP;  // Number of averaged samples for pwr0.
    };

class radioCESSBtransmit_F32 : public AudioStream_F32  {
//GUI: inputs:1, outputs:2 //this line used for automatic generation of GUI node
//GUI: shortName:CESSBTransmit  //this line used for automatic generation of GUI node
public:
    radioCESSBtransmit_F32(void) :
       AudioStream_F32(1, inputQueueArray_f32)
         {
		 setSampleRate_Hz(AUDIO_SAMPLE_RATE);
		 //uses default AUDIO_SAMPLE_RATE from AudioStream.h
		 //setBlockLength(128);   Always default 128
	     }

    radioCESSBtransmit_F32(const AudioSettings_F32 &settings) :
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
		    Serial.print("Status, decimate init = ");  Serial.println(
                arm_fir_decimate_init_f32(&decimateInst, 65, 4,
                   (float32_t*)decimateFilter48, &pStateDecimate[0], 128) );

            // Putting this init stuff here is in anticipation of
            // adding 96 ksps support later.
            arm_fir_init_f32(&firInstWeaverI, 213, (float32_t*)weaverFilter,
                   &pStateWeaverI[0], nW);
            arm_fir_init_f32(&firInstWeaverQ, 213, (float32_t*)weaverFilter,
                   &pStateWeaverQ[0], nW);

            arm_fir_init_f32(&firInstInterpolate1I, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate1I[0], nC);
            arm_fir_init_f32(&firInstInterpolate1Q, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate1Q[0], nC);

            arm_fir_init_f32(&firInstClipperI, 213, (float32_t*)weaverFilter,
                   &pStateClipperI[0], nC);
            arm_fir_init_f32(&firInstClipperQ, 213, (float32_t*)weaverFilter,
                   &pStateClipperQ[0], nC);

            arm_fir_init_f32(&firInstOShootI, 125, (float32_t*)osFilter,
                   &pStateOShootI[0], nC);
            arm_fir_init_f32(&firInstOShootQ, 125, (float32_t*)osFilter,
                   &pStateOShootQ[0], nC);

            arm_fir_init_f32(&firInstInterpolate2I, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate2I[0], nC);
            arm_fir_init_f32(&firInstInterpolate2Q, 23, (float32_t*)interpolateFilter1,
                   &pStateInterpolate2Q[0], nC);

		    }
/*		else if(sample_rate_Hz>88000.0f && sample_rate_Hz<100100.0f)
            {
			// GET THINGS WORKING AT SAMPLE_RATE_44_50 FIRST AND THEN FIX UP 96 ksps
			// Design point is 96 ksps
		    }
 */
		else
            {
			// Unsupported sample rate
			sampleRate = SAMPLE_RATE_0;
            nW = 1;
            nC = 1;
		    }
        phaseIncrementW = 512.0f * freqW / 12000.0f;   // 57.6 for 12ksps
        newLevelDataReady = false;
        }

    struct levels* getLevels(int what) {
		if(what != 0)  // 0 leaves a way to get pointer before data is ready
 		    {
            levelData.pwr0 = powerSum0/(2.975f*(float32_t)countPower0);  // WHY????
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

    // The LSB/USB selection depends on the processing of the
    // IQ outputs of this class.  But, what we can do here is to reverse the
    // selectio by reversing the phase of one of the Weaver LO's.
    void setSideband(bool _sbReverse)
        {
        sidebandReverse = _sbReverse;
	    }

    virtual void update(void);

private:
    void sincos(float32_t ph);
    struct levels levelData;
    audio_block_f32_t *inputQueueArray_f32[1];
    float32_t freqW = 1350.0f;  // Set here and not changed

    // Input/Output is at 48 (or later 96 ksps).  Weaver generation is at 12 ksps.
    // Clipping and overshoot processing is at 24 ksps.
    // Next line is to indicate that setSampleRateHz() has not executed
    int sampleRate = SAMPLE_RATE_0;
    float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;   // 44.1 ksps
    int16_t nW = 32;  // 32 or 16
    int16_t nC = 64;  // 64 or 32
    float32_t phaseIncrementW = 512.0f * freqW / 24000.0f;
    float32_t phaseW = 0.0f;  // Weaver signal 0.0 to 512.0
    uint16_t  block_length = 128;
    bool sidebandReverse = false;

    float32_t pStateDecimate[128 + 65 - 1];    // Goes with CMSIS decimate function
    arm_fir_decimate_instance_f32 decimateInst;

    float32_t pStateWeaverI[32 + 213 - 1];    // Goes with Weaver filter out
    arm_fir_instance_f32 firInstWeaverI;      // at 12 ksps
    float32_t pStateWeaverQ[32 + 213 - 1];
    arm_fir_instance_f32 firInstWeaverQ;


    float32_t pStateInterpolate1I[64 + 23 - 1];  // For interpolate 12 to 24 ksps
    arm_fir_instance_f32 firInstInterpolate1I;
    float32_t pStateInterpolate1Q[64 + 23 - 1];
    arm_fir_instance_f32 firInstInterpolate1Q;


    float32_t pStateClipperI[64 + 213 - 1];    // Goes with Clipper filter
    arm_fir_instance_f32 firInstClipperI;      // at 24 ksps
    float32_t pStateClipperQ[64 + 213 - 1];
    arm_fir_instance_f32 firInstClipperQ;


    float32_t pStateOShootI[64+125-1];  // 129-1];
    arm_fir_instance_f32 firInstOShootI;
    float32_t pStateOShootQ[64+125-1];
    arm_fir_instance_f32 firInstOShootQ;

    float32_t pStateInterpolate2I[128 + 23 - 1];  // For interpolate 12 to 24 ksps
    arm_fir_instance_f32 firInstInterpolate2I;
    float32_t pStateInterpolate2Q[128 + 23 - 1];
    arm_fir_instance_f32 firInstInterpolate2Q;

    float32_t sn, cs;
    float32_t gainIn = 1.0f;
    float32_t gainCompensate = 2.0f;
    float32_t gainOut = 1.0f;

    // In the overshoot compensator, we need to search for the highest
    // filter output over several samples.
    // And a tiny delay to allow negative time for the previous path
    float32_t osEnv[4];
    uint16_t indexOsEnv = 0;  // 0 to 3 by using a 2-bit mask

    // We need a delay for overshoot remove to account for the FIR
    // filter in the correction path. Making the delay array
    // exactly 2^6=64 allows a simple circular structure.
    float32_t osDelayI[64];
    float32_t osDelayQ[64];
    uint16_t indexOsDelay = 0;

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
    // uint16_t ny = 0;  // For test pulse generation

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

/* FIR filter for Weaver I & Q
 * Filter designed with http://t-filter.appspot.com
 * Sampling frequency: 12000 ksps
 *    0 Hz - 1350 Hz  ripple = 0.14 dB
 * 1500 Hz - 6000 Hz  atten = -60.2 dB
 * ALSO: 0 to 2700 Hz at 24 ksps    */
const float32_t weaverFilter[213] = {
 0.00069446f, 0.00037170f, 0.00016640f,-0.00025667f,-0.00077930f,-0.00120663f,
-0.00134867f,-0.00111550f,-0.00057687f, 0.00005147f, 0.00049736f, 0.00056149f,
 0.00022366f,-0.00033377f,-0.00080586f,-0.00091552f,-0.00056344f, 0.00010449f,
 0.00075723f, 0.00104136f, 0.00077294f, 0.00005168f,-0.00076730f,-0.00124489f,
-0.00108978f,-0.00033029f, 0.00067306f, 0.00139546f, 0.00142002f, 0.00067429f,
-0.00050084f,-0.00150186f,-0.00176980f,-0.00109852f, 0.00022372f, 0.00153080f,
 0.00211108f, 0.00159111f, 0.00016633f,-0.00146039f,-0.00242101f,-0.00214184f,
-0.00067864f, 0.00126494f, 0.00267008f, 0.00273272f, 0.00131711f,-0.00091957f,
-0.00282456f,-0.00333871f,-0.00207907f, 0.00040237f, 0.00284896f, 0.00392959f,
 0.00295636f, 0.00030577f,-0.00270677f,-0.00447189f,-0.00393839f,-0.00122551f,
 0.00235504f, 0.00492259f, 0.00500607f, 0.00237350f,-0.00174927f,-0.00523381f,
-0.00613636f,-0.00376725f, 0.00083831f, 0.00534869f, 0.00730076f, 0.00542689f,
 0.00043859f,-0.00520046f,-0.00846933f,-0.00738444f,-0.00216395f, 0.00470259f,
 0.00960921f, 0.00969387f, 0.00446038f,-0.00373274f,-0.01068416f,-0.01245333f,
-0.00752832f, 0.00210318f, 0.01166261f, 0.01586953f, 0.01175214f, 0.00053376f,
-0.01251222f,-0.02039576f,-0.01795974f,-0.00492844f, 0.01320402f, 0.02719248f,
 0.02832779f, 0.01314687f,-0.01371714f,-0.04016441f,-0.05091338f,-0.03387251f,
 0.01403178f, 0.08421962f, 0.15843610f, 0.21483324f, 0.23586349f, 0.21483324f,
 0.15843610f, 0.08421962f, 0.01403178f,-0.03387251f,-0.05091338f,-0.04016441f,
-0.01371714f, 0.01314687f, 0.02832779f, 0.02719248f, 0.01320402f,-0.00492844f,
-0.01795974f,-0.02039576f,-0.01251222f, 0.00053376f, 0.01175214f, 0.01586953f,
 0.01166261f, 0.00210318f,-0.00752832f,-0.01245333f,-0.01068416f,-0.00373274f,
 0.00446038f, 0.00969387f, 0.00960921f, 0.00470259f,-0.00216395f,-0.00738444f,
-0.00846933f,-0.00520046f, 0.00043859f, 0.00542689f, 0.00730076f, 0.00534869f,
 0.00083831f,-0.00376725f,-0.00613636f,-0.00523381f,-0.00174927f, 0.00237350f,
 0.00500607f, 0.00492259f, 0.00235504f,-0.00122551f,-0.00393839f,-0.00447189f,
-0.00270677f, 0.00030577f, 0.00295636f, 0.00392959f, 0.00284896f, 0.00040237f,
-0.00207907f,-0.00333871f,-0.00282456f,-0.00091957f, 0.00131711f, 0.00273272f,
 0.00267008f, 0.00126494f,-0.00067864f,-0.00214184f,-0.00242101f,-0.00146039f,
 0.00016633f, 0.00159111f, 0.00211108f, 0.00153080f, 0.00022372f,-0.00109852f,
-0.00176980f,-0.00150186f,-0.00050084f, 0.00067429f, 0.00142002f, 0.00139546f,
 0.00067306f,-0.00033029f,-0.00108978f,-0.00124489f,-0.00076730f, 0.00005168f,
 0.00077294f, 0.00104136f, 0.00075723f, 0.00010449f,-0.00056344f,-0.00091552f,
-0.00080586f,-0.00033377f, 0.00022366f, 0.00056149f, 0.00049736f, 0.00005147f,
-0.00057687f,-0.00111550f,-0.00134867f,-0.00120663f,-0.00077930f,-0.00025667f,
 0.00016640f, 0.00037170f, 0.00069446f};

/* FIR for filtering limiter and overshoot correction
 * FIR filter designed with http://t-filter.appspot.com
 * Sampling frequency: 24000 Hz
 * 0 Hz-1400 Hz       gain=1  ripple=0.07 dB
 * 1820 Hz-12000 Hz   attenuation=40.4 dB
 */
float32_t osFilter[125] = {
//-0.00207432f, 0.00402547f,
                           0.00200766f, 0.00106812f, 0.00044566f,-0.00014761f,
-0.00074036f,-0.00129580f,-0.00169464f,-0.00183414f,-0.00164520f,-0.00111129f,
-0.00029199f, 0.00069623f, 0.00168197f, 0.00246922f, 0.00287793f, 0.00277706f,
 0.00212434f, 0.00097933f,-0.00049561f,-0.00205243f,-0.00339945f,-0.00424955f,
-0.00438005f,-0.00368304f,-0.00219719f,-0.00011885f, 0.00222062f, 0.00440171f,
 0.00598772f, 0.00660803f, 0.00603436f, 0.00424134f, 0.00143235f,-0.00197384f,
-0.00539709f,-0.00818867f,-0.00974422f,-0.00962242f,-0.00764568f,-0.00396213f,
 0.00094275f, 0.00629665f, 0.01114674f, 0.01451066f, 0.01555071f, 0.01374059f,
 0.00899944f, 0.00176454f,-0.00701380f,-0.01596042f,-0.02344211f,-0.02778959f,
-0.02754621f,-0.02170618f,-0.00990373f, 0.00747576f, 0.02928698f, 0.05372275f,
 0.07850988f, 0.10117969f, 0.11937421f, 0.13114808f, 0.13522153f, 0.13114808f,
 0.11937421f, 0.10117969f, 0.07850988f, 0.05372275f, 0.02928698f, 0.00747576f,
-0.00990373f,-0.02170618f,-0.02754621f,-0.02778959f,-0.02344211f,-0.01596042f,
-0.00701380f, 0.00176454f, 0.00899944f, 0.01374059f, 0.01555071f, 0.01451066f,
 0.01114674f, 0.00629665f, 0.00094275f,-0.00396213f,-0.00764568f,-0.00962242f,
-0.00974422f,-0.00818867f,-0.00539709f,-0.00197384f, 0.00143235f, 0.00424134f,
 0.00603436f, 0.00660803f, 0.00598772f, 0.00440171f, 0.00222062f,-0.00011885f,
-0.00219719f,-0.00368304f,-0.00438005f,-0.00424955f,-0.00339945f,-0.00205243f,
-0.00049561f, 0.00097933f, 0.00212434f, 0.00277706f, 0.00287793f, 0.00246922f,
 0.00168197f, 0.00069623f,-0.00029199f,-0.00111129f,-0.00164520f,-0.00183414f,
-0.00169464f,-0.00129580f,-0.00074036f,-0.00014761f, 0.00044566f, 0.00106812f,
 0.00200766f};
//           0.00402547f,-0.00207432f};

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

/* Linear phase baseband filter
 * FIR filter designed with http://t-filter.appspot.com
 * Sampling frequency: 24000 Hz
 * 0 Hz - 1420 Hz      ripple = 0.146 dB
 * 1700 Hz - 12000 Hz  attenuation = -50.1 dB  */
float32_t basebandFilter[199] = {
 0.00196058f, 0.00082632f, 0.00085733f, 0.00078043f, 0.00059145f, 0.00030448f,
-0.00004829f,-0.00042015f,-0.00075631f,-0.00100164f,-0.00110987f,-0.00105351f,
-0.00083052f,-0.00046510f,-0.00000746f, 0.00047037f, 0.00089019f, 0.00117401f,
 0.00126254f, 0.00112385f, 0.00076287f, 0.00022299f,-0.00041828f,-0.00105968f,
-0.00159130f,-0.00191324f,-0.00195342f,-0.00168166f,-0.00111897f,-0.00033785f,
 0.00054658f, 0.00139192f, 0.00205194f, 0.00240019f, 0.00235381f, 0.00189072f,
 0.00105796f,-0.00003104f,-0.00121055f,-0.00228720f,-0.00307062f,-0.00340596f,
-0.00320312f,-0.00245657f,-0.00125253f, 0.00023880f, 0.00178631f, 0.00313236f,
 0.00403460f, 0.00430822f, 0.00386101f, 0.00271591f, 0.00101544f,-0.00099378f,
-0.00299483f,-0.00464878f,-0.00565026f,-0.00578103f,-0.00495344f,-0.00323470f,
-0.00084708f, 0.00185766f, 0.00444725f, 0.00647565f, 0.00755579f, 0.00742946f,
 0.00601997f, 0.00345944f, 0.00008392f,-0.00360622f,-0.00701534f,-0.00954279f,
-0.01068201f,-0.01011191f,-0.00776604f,-0.00386666f, 0.00108417f, 0.00636072f,
 0.01110737f, 0.01446572f, 0.01570891f, 0.01437252f, 0.01035602f, 0.00397827f,
-0.00402157f,-0.01254475f,-0.02025120f,-0.02572083f,-0.02763900f,-0.02498240f,
-0.01717994f,-0.00422067f, 0.01329264f, 0.03419240f, 0.05682312f, 0.07923505f,
 0.09938512f, 0.11536507f, 0.12562657f, 0.12916328f, 0.12562657f, 0.11536507f,
 0.09938512f, 0.07923505f, 0.05682312f, 0.03419240f, 0.01329264f,-0.00422067f,
-0.01717994f,-0.02498240f,-0.02763900f,-0.02572083f,-0.02025120f,-0.01254475f,
-0.00402157f, 0.00397827f, 0.01035602f, 0.01437252f, 0.01570891f, 0.01446572f,
 0.01110737f, 0.00636072f, 0.00108417f,-0.00386666f,-0.00776604f,-0.01011191f,
-0.01068201f,-0.00954279f,-0.00701534f,-0.00360622f, 0.00008392f, 0.00345944f,
 0.00601997f, 0.00742946f, 0.00755579f, 0.00647565f, 0.00444725f, 0.00185766f,
-0.00084708f,-0.00323470f,-0.00495344f,-0.00578103f,-0.00565026f,-0.00464878f,
-0.00299483f,-0.00099378f, 0.00101544f, 0.00271591f, 0.00386101f, 0.00430822f,
 0.00403460f, 0.00313236f, 0.00178631f, 0.00023880f,-0.00125253f,-0.00245657f,
-0.00320312f,-0.00340596f,-0.00307062f,-0.00228720f,-0.00121055f,-0.00003104f,
 0.00105796f, 0.00189072f, 0.00235381f, 0.00240019f, 0.00205194f, 0.00139192f,
 0.00054658f,-0.00033785f,-0.00111897f,-0.00168166f,-0.00195342f,-0.00191324f,
-0.00159130f,-0.00105968f,-0.00041828f, 0.00022299f, 0.00076287f, 0.00112385f,
 0.00126254f, 0.00117401f, 0.00089019f, 0.00047037f,-0.00000746f,-0.00046510f,
-0.00083052f,-0.00105351f,-0.00110987f,-0.00100164f,-0.00075631f,-0.00042015f,
-0.00004829f, 0.00030448f, 0.00059145f, 0.00078043f, 0.00085733f, 0.00082632f,
 0.00196058};

};      // end Class
#endif
