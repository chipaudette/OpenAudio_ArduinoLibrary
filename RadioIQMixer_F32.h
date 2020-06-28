/*
 * RadioIQMixer_F32
 * 8 April 2020     Bob Larkin
 * With much credit to:
 * Chip Audette (OpenAudio) Feb 2017
 * and of course, to PJRC for the Teensy and Teensy Audio Library
 *
 * A basic building block is a pair of mixers fed in parallel with the
 * LO  going to the mixers at the same frequency, but differing in phase 
 * by 90 degrees.  This provides two outputs I and Q that are offset in
 * frequency but also 90 degrees apart in phase.  The LO are included
 * in the block, but there are no post-mixing filters.
 * 
 * The frequency is set by  .frequency(float freq_Hz)
 * Particularly for use in transmitting, there is provision for varying
 * the phase between the sine and cosine oscillators.  Technically this is no
 * longer sin and cos, but that is what real hardware needs.
 * 
 * The output levels are 0.5 times the input level.
 * 
 * Status:  Tested in doSimple==1
 *          Tested in FineFreqShift_OA.ino, T3.6 and T4.0 
 * 
 * Inputs:  0 is signal
 * Outputs: 0 is I     1 is Q
 * 
 * Functions, available during operation:
 *   void frequency(float32_t fr)  Sets BFO frequency Hz
 *   void iqmPhaseS(float32_t ps)  Sets Phase of Sine in radians
 *   void phaseS_C_r(float32_t pc) Sets relative phase of Cosine in radians, approximately pi/2
 *   void amplitudeC(float32_t a)  Sets relative amplitude of Sine, approximately 1.0
 *   void useSimple(bool s)        Faster if 1, but no phase/amplitude adjustment
 *   void setSampleRate_Hz(float32_t fs_Hz)  Allows dynamic sample rate change for this function
 * 
 * Time: T3.6 For an update of a 128 sample block, doSimple=1, 46 microseconds
 *       T4.0 For an update of a 128 sample block, doSimple=1, 20 microseconds
 */
 
#ifndef _radioIQMixer_f32_h
#define _radioIQMixer_f32_h

#include "AudioStream_F32.h"
#include "arm_math.h"
#include "mathDSP_F32.h"

class RadioIQMixer_F32 : public AudioStream_F32 {
//GUI: inputs:2, outputs:2  //this line used for automatic generation of GUI node
//GUI: shortName: IQMixer
public:
    // Option of AudioSettings_F32 change to block size or sample rate:
    RadioIQMixer_F32(void) :  AudioStream_F32(1, inputQueueArray_f32) {
      // Defaults
    }
    RadioIQMixer_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) {
	    setSampleRate_Hz(settings.sample_rate_Hz);
	    block_size = settings.audio_block_samples;
    }

    void frequency(float32_t fr) {    // Frequency in Hz
		freq = fr;
        if (freq < 0.0f) freq = 0.0f;
        else if (freq > sample_rate_Hz/2.0f) freq = sample_rate_Hz/2.0f;
        phaseIncrement = 512.0f * freq / sample_rate_Hz;
    }

    /* Externally, phase comes in the range (0,2*M_PI) keeping with C math functions
     * Internally,  the full circle is represented as (0.0, 512.0).  This is
     * convenient for finding the entry to the sine table.
     */
    void iqmPhaseS(float32_t a) {
        while (a < 0.0f) a += MF_TWOPI;
        while (a > MF_TWOPI) a -= MF_TWOPI;
        phaseS = 512.0f * a / MF_TWOPI;
        doSimple = false;
        return;
    }
    
    // phaseS_C_r  is the number of radians that the cosine output leads the
    // sine output.  The default is M_PI_2 = pi/2 = 1.57079633 radians,
    // corresponding to 90.00 degrees cosine leading sine.
    void iqmPhaseS_C(float32_t a) {
        while (a < 0.0f) a += MF_TWOPI;
        while (a > MF_TWOPI) a -= MF_TWOPI;
        // Internally a full circle is 512.00 of phase
        phaseS_C = 512.0f * a / MF_TWOPI;
        doSimple = false;
        return;
    }

    // The amplitude, a, is the peak, as in zero-to-peak.  This produces outputs
    // ranging from -a to +a.  Both outputs are the same amplitude.
    void iqmAmplitude(float32_t a) {
        amplitude_pk = a;
        doSimple = false;
        return;
    }

     // Speed up calculations by setting phaseS_C=90deg, amplitude=1
     void useSimple(bool s) {
        doSimple = s;
        if(doSimple) {
			phaseS_C = 128.0f;
			amplitude_pk = 1.0f;
	    }
        return;
    }   

    void setSampleRate_Hz(float32_t fs_Hz) {
        // Check freq range
        if (freq > sample_rate_Hz/2.0f) freq = sample_rate_Hz/2.f;
        // update phase increment for new frequency
        phaseIncrement = 512.0f * freq / fs_Hz;
    }
      
    void showError(uint16_t e) {    // Serial.print errors in update()
        errorPrintIQM = e;
    }
       
    virtual void update(void);

private:
    audio_block_f32_t *inputQueueArray_f32[1];
    float32_t freq = 1000.0f;
    float32_t phaseS = 0.0f;
    float32_t phaseS_C = 128.00;    // 512.00 is 360 degrees
    float32_t amplitude_pk = 1.0f;
    float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE_EXACT;
    float32_t phaseIncrement = 512.00f * freq /sample_rate_Hz;
    uint16_t block_size = AUDIO_BLOCK_SAMPLES;
    uint16_t errorPrintIQM = 0;   // Normally off
    // if only freq() is used, the complexities of phase, phaseS_C,
    // and amplitude are not used, speeding up the sin and cos:
    bool      doSimple = true;
 
};

#endif

