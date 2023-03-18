/*
 * analyze_CTCSS_F32.h   For the OpenAudio_TeensyArduino library
 * (floating point audio).
 *   MIT License on changed portions
 *   Bob Larkin March 2021 original write
 *   Rev 21 Jan 2022 Major redo and ready to publish.  Bob L
 *   Rev 15 March 2023 - Added dynamic sample rate control; added 12 ksps; corrected
 *      receiveWritable_f32() for block with output.  Bob L
 *
 * This is specific for the CTCSS tone system
 * with tones in the 67.0 to 254.1 Hz range.
 *
 * Parts of this are based on analyze_toneDetect:  Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* The tone frequency is set by the function frequency(float freq, float tMeas).
 * The parameter freq is in Hz.  The parameter tMeas is optional and is the
 * measuring time for the Goertzel algorithm, measured in milliseconds. Sort
 * tMeas, say 100, give fast response, but poor selectivity to adjacent tone
 * frequencies. Long values, say 500 to 1000, are sluggish and for NBFM systems
 * tend to omit initial words.  The default  for tMeas is 300 milliseconds.
 *
 *  CTCSS ranges from 67.0 to 254.1 Hz
 (Actual CTCSS use seems to be 77.0 to 203.5 Hz)
 This object can be used on any frequency in the range.
 But, the standard CTCSS channel numbers, designations and
 frequencies, in Hz, are:
 1   XZ  67          25  5A  156.7
 39  WZ  69.3        40      159.8
 2   XA  71.9        26  5B  162.2
 3   WA  74.4        41      165.5
 4   XB  77          27  6Z  167.9
 5   WB  79.7        42      171.3
 6   YZ  82.5        28  6A  173.8
 7   YA  85.4        43      177.3
 8   YB  88.5        29  6B  179.9
 9   ZZ  91.5        44      183.5
 10  ZA  94.8        30  7Z  186.2
 11  ZB  97.4        45      189.9
 12  1Z  100         31  7A  192.8
 13  1A  103.5       46      196.6
 14  1B  107.2       47      199.5
 15  2Z  110.9       32  M1  203.5
 16  2A  114.8       48  8Z  206.5
 17  2B  118.8       33  M2  210.7
 18  3Z  123         34  M3  218.1
 19  3A  127.3       35  M4  225.7
 20  3B  131.8       49  9Z  229.1
 21  4Z  136.5       36  M5  233.6
 22  4A  141.3       37  M6  241.8
 23  4B  146.2       38  M7  250.3
 24  5Z  151.4       50  0Z  254.1
 */

/* FUTURE (Let's see this all working without windowing, first):
 *  This CTCSS detector has an optional windowing function.  The default is
 * no windowing, but this leaves the first Goertzel filter
 * sideband at about -13 dB relative to the tone frequency response.  See
 * setGWindow(float *pWinCoeff) below.  The caller supplied window function is
 * an array of NWINDOW floats corresponding to half of the window
 * function, starting with the outside (non-center) coefficient.  The number
 * of coefficients depends on the sample rate of the ADC and the amount
 * time to make a detector measurement.  The following might be used in an
 * .ino to set the size of the half array.  This is in pre-compiler lingo
 * as it needs to be installed at compile time, not run time:
 *    #define  SAMPLE_RATE  44117.0f
 *    #define  DETECTOR_TIME  300.0f
 *    #define  NWINDOW  (uint16_t)( 0.5 + SAMPLE_RATE * DETECTOR_TIME / 32000.0f )
 * See example/ToneDetect2.  Note that DETECTOR_TIME is in milliseconds.
 */

/* This class can be used to see if a sub-audible tone is present with resulting
 * action coming through the .ino.  This uses available() and readTonePresent().
 * Alternatively, the threshold(s) can be set
 * and the comparison results used to gate the input signal through to other
 * audio blocks.  Done this way, the audio gating is done without
 * any .ino processing.
 *
 * Note - Both the Goertzel tone output and the reference band outputs are in
 * "power."  This is convenient and meaningful for the CTCSS detector, but
 * different than the Teensy library tone detectors, which take the square
 * root of the power for output.
 *
 * Two outputs are available, TonePower and RefPower.  TonePower is the output
 * of the Goertzel sharply tuned filter and is the conventional CTCSS output
 * that is compared to a threshold.  RefPower measures the power in the entire
 * 67 to 254 Hz sub-audible band, except at the tone frequency. This is a measure
 * of both FM quieting and also the presence of a tone away from the TonePower
 * frequency.  By comparing TonePower to RefPower several dB of increased
 * sensitivity is achieved for threshold setting.
 *
 * Thresholds are used in two ways.  FirstThey can be read by the .ino
 * with readTonePresent(what) where "what" allows an absolute threshod comparison
 * on Tone Power, a relative comparison between Tone Power and RefPower,
 * or both.  The relative comparison is the preferred form for many applicatins.
 * Parameter "what" can be isAbsThreshold, isRelThreshold or isBothThreshold.
 *
 * The second method of threshold useage is to gate the input of the block to
 * output 0 of the block.  This happens as part of the class update and requires
 * no .ino action.  The control comes from the function
 * thresholds(levelAbs, levelRel).  Setting either levelbs or levelRel to zero
 * removes that measure from the gating process.  So, a typical function call
 * would be detCTCSS.thresholds(0.0f, 0.4f);
 *
 * This class uses decimation-by-16 to improve the performance of the low-frequency
 * filtering and to reduce processing load.  The filter parameters of the decimation
 * LPF can be used at any sampling rate from 44 to 100 kHz.  Other rates will need
 * a new design for the LPF.  See the discussion below for "iirLpfCoeffs" showing
 * how to design such a filter.  (Mar 2023: Decimate by 8 for 12 ksps).
 *
 * The bandpass filter to cover 67 to 254 Hz is predesigned for sample rates near
 * 44, 48, 96 or 100 kHz.  The discussion below "BPF iir Coefficients" has
 * information for designing BPF for other sample rates.  The new design is entered
 * via the function setCTCSS_BP(float *filterCoeffs).
 *
 * Each update of an 128-input block takes about 42 uSec on a Teensy 3.6.
 *
 * Note - Pieces of this support block sizes other than 128, but not all.
 * Work needs to be done to implement and test variable block size.  The same is
 * true for sample rates outside the 12 and 44.1 to 100 kHz range.
 */

#ifndef analyze_CTCSS_F32_h_
#define analyze_CTCSS_F32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"

class analyze_CTCSS_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:DetCTCSS
public:
    analyze_CTCSS_F32(void)
      : AudioStream_F32(1, inputQueueArray_f32) {
          sample_rate_Hz = AUDIO_SAMPLE_RATE;
          block_size = AUDIO_BLOCK_SAMPLES;
          initCTCSS();
          }
    // Option of AudioSettings_F32 change to block size and/or sample rate:
    analyze_CTCSS_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1,inputQueueArray_f32){
        sample_rate_Hz = settings.sample_rate_Hz;
        block_size = settings.audio_block_samples;
        initCTCSS();
        }

    // This is called at instantation and also if either freq or sample_rate_Hz is changed
    void initCTCSS(void)
        {
        gEnabled = true;
        float32_t nDecimatef = (float32_t)nDecimate;  // Later use
        // INFO: Initialize BiQuad instance (ARM DSP Math Library)
        //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html

        // First, select the BPF for measuring the noise level
        if(iirUserBpfCoeffs != NULL)
           iirBpfCoeffs = iirUserBpfCoeffs;    // From INO, always use if supplied
        //else if(sample_rate_Hz>11000.0f && sample_rate_Hz<11050.0f)
           //iirBpfCoeffs = coeff32bp11;    Not currently implemented
        else if(sample_rate_Hz>11975.0f && sample_rate_Hz<12025.0f)
           iirBpfCoeffs = coeff32bp12;
        //else if(sample_rate_Hz>23950.0f && sample_rate_Hz<24050.0f)
           //iirBpfCoeffs = coeff32bp24;    Not currently implemented
        else if(sample_rate_Hz>43900.0f && sample_rate_Hz<44200.0f)
           iirBpfCoeffs = coeff32bp44;
        else if(sample_rate_Hz>47900.0f && sample_rate_Hz<48100.0f)
           iirBpfCoeffs = coeff32bp48;
        else if(sample_rate_Hz>95800.0f && sample_rate_Hz<96200.0f)
           iirBpfCoeffs = coeff32bp96;
        else if(sample_rate_Hz>99800.0f && sample_rate_Hz<100200.0f)
           iirBpfCoeffs = coeff32bp100;
        else gEnabled = false;
        arm_biquad_cascade_df1_init_f32(&iir_bpf_inst,   4, iirBpfCoeffs,       &StateBpf32[0]);

        if(sample_rate_Hz>11900 && sample_rate_Hz<24100)  // Now choose a pre-decimation LPF
           {
           nDecimate = 8;
           arm_biquad_cascade_df1_init_f32(&iir_lpf_inst, 4, &iirLpfCoeffs12[0],   &StateLpf32[0]);
           }
        else if (sample_rate_Hz>44000 && sample_rate_Hz<100100)
           {
           nDecimate = 16;
           arm_biquad_cascade_df1_init_f32(&iir_lpf_inst,  4, &iirLpfCoeffs44[0],   &StateLpf32[0]);
           }
        else gEnabled = false;
        nDecimatef = (float32_t)nDecimate;
        sampleRate2 = sample_rate_Hz / nDecimatef;

        for (int i=0; i<16; i++)   // Need 8 when decimate is 16 or 16 if decimate is 8
            {
			d16a[i] = 0.0f;
			d16b[i] = 0.0f;
		    }
        arm_biquad_cascade_df1_init_f32(&iir_nulls_inst, 4, &iirNullsCoeffs[0], &StateNullsF32[0]);

        // e.g. freq=103.5, tMeas=250, sample_rate_Hz = 44100, gLength=689
        gLength = (uint16_t)(0.5 + sample_rate_Hz*tMeas/(1000.0f*nDecimatef));  // reinstated
        // Now set the notch filters for the four-biquad notch filters
        setNotchX4(freq);

        // e.g., freq=103.5, sample_rate_Hz=44100, gCoefficient=2*cos(0.23594)=1.944634
        gCoefficient = 2.0f*cosf(6.28318530718f*nDecimatef*freq/sample_rate_Hz);
        // Another constant for ending calculation is exp(-j 2*pi*16*freq*(glength-1)/sample_rate_Hz)
        ccRe =  cos(6.28318530718f*nDecimatef*freq/sample_rate_Hz);
        ccIm = -sin(6.28318530718f*nDecimatef*freq/sample_rate_Hz);

        thresholds(0.1f, 1.0f);
    }

    // Set frequency and length of Goertzel detect tone in Hz and milliseconds.
    void frequency(float32_t _freq) {      // Defaults to 300 mSec
		frequency(_freq, 300.0f);
        }
    void frequency(float32_t _freq, float32_t _tMeas) {
        freq = _freq;
        tMeas = _tMeas;
        initCTCSS();
    }

    // Sample rates 12, 44.1, 48, 96 and 100 ksps are supported.
    void setSampleRate_Hz(const float32_t &fs_Hz) {
       sample_rate_Hz = fs_Hz;
       initCTCSS();
    }

    bool available(void) {
        __disable_irq();
        bool flag = new_output;
        if(flag)
            new_output = false;     // Prevent multiple reads of same data
        __enable_irq();
        return flag;
        }

    float32_t readTonePower(void) { return powerTone; }

    float32_t readRefPower(void)  { return powerRef; }

    const uint16_t isAbsThreshold=1;
    const uint16_t isRelThreshold=2;
    const uint16_t isBothThreshold=3;
    bool  readTonePresent(uint16_t what) {
		if(what == isAbsThreshold)
		   return (powerTone>threshAbs);
		else if(what == isRelThreshold)
		   return (powerTone>threshRel*powerRef);
		else if(what == isBothThreshold)
		   return (powerTone>threshAbs  &&  powerTone>threshRel*powerRef);
        else return false;    // Something was wrong about call
	    }

    void thresholds(float32_t levelAbs, float32_t levelRel) {
		// The absolute threshold is compared with tonePower
        if (levelAbs < 0.000001f) threshAbs = 0.000001f;
        else threshAbs = levelAbs;

        // The relative threshold is multiples of refPower
        if (levelRel < 0.000001f) threshRel = 0.000001f;
        else threshRel = levelRel;
        }

    operator bool();  // true if at or above threshold, false if below

    virtual void update(void);

    /* The bandpass filter covers 67 to 254 Hz to allow a comparison
     * measurement for the tone.  This changes with sample rate. Five
     * precomputed filters are provided for 12, 44.1, 48, 96 and 100 kHz.
     * Supply a NULL for the filterCoeffs to use these five.
     * _filterCoeffs points to an array of 20 floats that are IIR
     * coefficients for four BiQuad Stages.  To use caller supplied
     * filter coefficients, call with a pointer to that 4x BiQuad filter at
     * filterCoeffs.
     */
    float* setCTCSS_BP(float32_t *_filterCoeffs)  {
        iirUserBpfCoeffs = _filterCoeffs;       // Caller supplied
       return iirBpfCoeffs;
       }

    /* To use a windowing fuction to change the Goertzel sidelobe levels,
     * in the .ino compute half the window function and put that in an array.
     * Call this function with a pointer to that array.  To stop windowing,
     * call this function with NULL as an argument.
     * /   Windowing not yet implemented  <<<-------------------
    void setGWindow(float32_t *pWinCoeff)
       {
       windowCoeff = pWinCoeff;    // Point to the caller supplied window function
       if(pWinCoeff != NULL)
          nWindow = (uint16_t)(0.5f + sample_rate_Hz*tMeas/(2.0f*1000.0f*16.0f));
       else
          nWindow = 0;
       }
     */

private:
    int16_t  count16 = 0;            unsigned long cnt = 0UL;
    float32_t  ccRe=0.0f;
    float32_t  ccIm=0.0f;
    float32_t freq = 103.5f;
    float32_t tMeas = 300.0f;
    uint16_t nDecimate = 16;
    float32_t sampleRate2 = 2756.25f;  // 44100/16 etc.
    float32_t d16a[16];             // Small data blocks, after decimation
    float32_t d16b[16];             // Reference power sum
    float32_t gCoefficient = 1.9f;  // Goertzel algorithm coefficient
    float32_t gs1, gs2;             // Goertzel algorithm state
    float32_t out1, out2;           // Goertzel algorithm state output
    float32_t powerTone;
    float32_t powerRef;
    uint16_t gLength;           // number of samples to analyze
    uint16_t gCount;            // how many left to analyze
    float32_t powerSum = 0.0f;
    uint16_t powerSumCount = 0;
    float32_t threshAbs = 0.1f;     // absolute threshold, 0.01 to 0.99
    float32_t threshRel = 1.0f;     // noise relative threshold
    bool gEnabled = false;
    volatile bool new_output = false;
    audio_block_f32_t *inputQueueArray_f32[1];
    float32_t sample_rate_Hz = AUDIO_SAMPLE_RATE;
    uint16_t block_size = 128;
    float32_t iirNullsCoeffs[20];
    float32_t *iirBpfCoeffs;
    float32_t *iirUserBpfCoeffs;

    // windowCoeff needs to store half of values (symmetry) for up to
    // 0.25 second at 100KHz sample rate, or 0.25*0.5*100000/16=782 pts.
    // This is extravagant and needs to be in caller area to allow
    // frugality when there is no window at all.
    // float32_t *windowCoeff;  <<- Windowing is not currently implemented
    // uint16_t nWindow = 0;  <<-   ditto
    /* Info - The structure from arm_biquad_casd_df1_inst_f32 instance consists of
     *    uint32_t  numStages;
     *    const float32_t *pCoeffs;  //Points to the array of coefficients, length 5*numStages.
     *    float32_t *pState;         //Points to the array of state variables, length 4*numStages.
     * We have 3 IIR's:
     */
    arm_biquad_casd_df1_inst_f32 iir_lpf_inst;
    arm_biquad_casd_df1_inst_f32 iir_bpf_inst;
    arm_biquad_casd_df1_inst_f32 iir_nulls_inst;

    /* Each Biquad stage has 4 state variables x[n-1], x[n-2], y[n-1], and y[n-2].
     * See https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html
     * The state variables are arranged in the pState array as:
     *   {x[n-1], x[n-2], y[n-1], y[n-2]}
     * The 4 state variables for stage 1 are first, then the 4 state variables for stage 2,
     * and so on. The state array has a total length of 4*numStages values.
     * The state variables are updated after each block of data is processed; the
     * coefficients are untouched.
     */
    float32_t  StateLpf32[4*4];
    float32_t  StateBpf32[4*4];
    float32_t  StateNullsF32[4*4];

    // The coefficients are stored in the array pCoeffs in the following order:
    //     {b10, b11, b12, a11, a12, b20, b21, b22, a21, a22, ...}
    // where b1x and a1x are the coefficients for the first stage, b2x and a2x are
    // the coefficients for the second stage, and so on. The pCoeffs array
    // contains a total of 5*numStages values.
    // a10, a20, etc are all 1.000 and not listed.
    /*
     * LPF for decimation before CTCSS tone detector.
     * Pick cutoff frequency to pass 254 Hz with fs=44.1kHz
     * or fc=2*254/44100=0.01152.  This passes highest tone
     * frequency at the lowest sample rate. These
     * were calculated by Iowa Hills routines.
     */
    //12000   260Hz -60 dB double elliptic 8 pole  -60 dB at 300 Hz 
    float32_t iirLpfCoeffs12[20] = {
     0.182145523f,-0.359763551f, 0.182145523f, 1.888598461f,-0.893125955f,
     0.350161286f,-0.689819799f, 0.350161286f, 1.922991947f,-0.933494720f,
     0.293698317f,-0.571683303f, 0.293698317f, 1.954562781f,-0.970276113f,
     0.053599924f,-0.089098167f, 0.053599924f, 1.973912800f,-0.992014481f };

    float32_t iirLpfCoeffs44[20] = {
     0.188636212f,-0.376924253f, 0.188636212f, 1.969403901f,-0.969752071f,
     0.357249908f,-0.713703189f, 0.357249908f, 1.980638291f,-0.981434917f,
     0.294316431f,-0.587455758f, 0.294316431f, 1.990628020f,-0.991805124f,
     0.049761875f,-0.098177786f, 0.049761875f, 1.996468755f,-0.997814718f};

     /* There is a band pass filter that is used after decimation to limit the
      * spectrum that is seen by the CTCSS detector to (67, 254) Hz. This
      * provides the entire CTCSS band to compare against to see if any other
      * tones are present.  It also restricts the amount of voice spectrum
      * presented to the CTCSS detector.  This filter is sample rate dependent
      * this routine allows for five different "built-in" rates.
      *
      *   Sample    LPF Decimation  CTCSS    BPF
      *    Rate     Fco   Ratio    Samp Rate Name
      *   ------- ------ --------  --------  -----
      *   12.0kHz  260Hz     8     1500.0Hz  BP12
      *   44.1kHz  254Hz    16     2756.2Hz  BP44
      *   48.0kHz  276Hz    16     3000.0Hz  BP48
      *   96.0kHz  553Hz    16     6000.0Hz  BP96
      *   100kHz   576Hz    16     6250.0Hz  BP100
      *
      * The following allow computing a bandpass filter for any sample rate in
      * the 44 to 100 kHz range (gnu octave script).
      *
% ==========  Sidebar:  BPF iir Coefficients =============================
% This GNU Octave m script will generate the BPF iir coefficients
% for other sample rates than the four listed here.  It will probably
% run under Matlab, but that is untested.
%
  format long e
  FSample = 6250.0;
  halfFSample = 0.5 * FSample
  [z,p,g] = ellip(4,0.1,60,[67 254]/halfFSample);
  sos = zp2sos(z,p,g);
  sos1 = sos(1:1,:)     % 1st row
  sos2 = sos(2:2,:)     % 2nd row
  sos3 = sos(3:3,:)     % 3rd row
  sos4 = sos(4:4,:)     % 4th row
%
% sos are the "Second Order State" (BiQuad) consisting of 4 sets of 6 numbers.
% These are numerator and denominator in form of the Open Audio Matlab
% IIR function setFilterCoeff_Matlab(float32_t b[], float32_t a[])
% in AudioFilterIIR_F32.h of
% https://github.com/chipaudette/OpenAudio_ArduinoLibrary/blob/master/AudioFilterIIR_F32.h
%
% sosi, i=1 to 4, are each 6 double precision numbers, a0, a1, a2,
% b0, b1, b2.  To use these with the ARM library,
% arm_biquad_cascade_df1_f32, two changes are needed.  The fourth
% number is a0 that is always 1.000 and for the arm routine, it is omitted.
% The fifth and sixth numbers are -a1 and -a2, so they need to have their
% signs reversed.  So, for example, for the 44.1 kHz sample rate:
% sos1 =
%   4.386558177463118e-03   4.835083945591431e-03   4.386558177463117e-03
%   1.000000000000000e+00  -1.503280453219001e+00   8.498498588788136e-01%
% sos2 =
%   1.000000000000000e+00  -4.234328155653108e-01   1.000000000000000e+00
%   1.000000000000000e+00  -1.524529304401012e+00   6.838502689975459e-01
% sos3 =
%   1.000000000000000e+00  -1.996808089693871e+00   9.999999999999998e-01
%   1.000000000000000e+00  -1.797650895821358e+00   8.356641238925192e-01
% sos4 =
%   1.000000000000000e+00  -1.999398698654737e+00   1.000000000000000e+00
%   1.000000000000000e+00  -1.941833848024410e+00   9.614543316508065e-01
% and the resulting arm coefficient array, including all 4 biquads, is
% coeff32BP44[20] = {
%   4.386558177463118e-03,  4.835083945591431e-03, 4.386558177463117e-03,
%   1.503280453219001e+00, -8.498498588788136e-01,
%   1.000000000000000e+00, -4.234328155653108e-01, 1.000000000000000e+00,
%   1.524529304401012e+00, -6.838502689975459e-01,
%   1.000000000000000e+00, -1.996808089693871e+00, 9.999999999999998e-01,
%   1.797650895821358e+00, -8.356641238925192e-01,
%   1.000000000000000e+00, -1.999398698654737e+00, 1.000000000000000e+00,
%   1.941833848024410e+00, -9.614543316508065e-01};
%   ===================================================================== */

    // The five predefined bandpass filters:
    // BP12  Decimated fSample = 1500 sps
    // 65 to 260, 0.1 dB rip, -60 dB elliptic, -7.5 dB at 300 Hz, -60 at 475
    float32_t coeff32bp12[20] =   {
        0.386816925f,-0.769638284f, 0.386816925f, 1.586751090f,-0.706113049f,
        0.265346713f, 0.247789162f, 0.265346713f, 0.996735842f,-0.484375851f,
        0.461314461f,-0.921736474f, 0.461314461f, 1.864126624f,-0.928392720f,
        0.380685155f, 0.664524969f, 0.380685155f, 0.720627184f,-0.766126702f};

    // BP44  Decimated fSample = 2756.2 Hz
    float32_t coeff32bp44[20] = {
        4.386558177463118e-03f,  4.835083945591431e-03f, 4.386558177463117e-03f,
        1.503280453219001e+00f, -8.498498588788136e-01f,
        1.000000000000000e+00f, -4.234328155653108e-01f, 1.000000000000000e+00f,
        1.524529304401012e+00f, -6.838502689975459e-01f,
        1.000000000000000e+00f, -1.996808089693871e+00f, 9.999999999999998e-01f,
        1.797650895821358e+00f, -8.356641238925192e-01f,
        1.000000000000000e+00f, -1.999398698654737e+00f, 1.000000000000000e+00f,
        1.941833848024410e+00f, -9.614543316508065e-01f};

    // BP48  Decimated fSample = 3000 Hz
    // 67 to 254 Band Pass 60 dB reject elliptic  -7.5 at 300 Hz -35 at 450 Hz
    float32_t coeff32bp48[20] = {
        0.272147027f,-0.543594385f, 0.272147027f, 1.819349787f,-0.850367415f,
        0.226417971f,-0.138869864f, 0.226417971f, 1.574952243f,-0.707479581f,
        0.261002559f,-0.521878771f, 0.261002559f, 1.949332941f,-0.965207141f,
        0.232822864f, 0.221820730f, 0.232822864f, 1.572088740f, -0.860996505}; 

    // BP96  Decimated fSample = 6000 Hz
    float32_t coeff32bp96[20] = {
        1.554430361573611e-03f,  5.765630035510897e-04f, 1.554430361573611e-03f,
        1.803990601116592e+00f, -8.406882700444890e-01f,
        1.000000000000000e+00f, -1.541034615722940e+00f, 9.999999999999998e-01f,
        1.846550880066576e+00f, -9.250235034256622e-01f,
        1.000000000000000e+00f, -1.999319784384852e+00f, 9.999999999999998e-01f,
        1.913429616511321e+00f, -9.218266406823739e-01f,
        1.000000000000000e+00f, -1.999871669777634e+00f, 1.000000000000000e+00f,
        1.978038062492293e+00f, -9.822341038950552e-01f};

    // BP100 Decimated fSample = 6250 Hz
    float32_t coeff32bp100[20] = {
        1.504180063396739e-03f, -6.773089598829345e-04f, 1.504180063396738e-03f,
        1.812635069874353e+00f, -8.465624856328144e-01f,
        1.000000000000000e+00f, -1.573630872142354e+00f, 1.000000000000000e+00f,
        1.855377871833802e+00f, -9.278512110118003e-01f,
        1.000000000000000e+00f, -1.999372984381045e+00f, 9.999999999999998e-01f,
        1.917101697268557e+00f, -9.248525551899550e-01f,
        1.000000000000000e+00f, -1.999881702874664e+00f, 1.000000000000000e+00f,
        1.979072537983060e+00f, -9.829412184208248e-01f};

    /* Designs a four-notch filter to null out a tone.  At 100 Hz the
     * null width of the four staggered tones is about 2Hz wide.
     * iirNullsCoeffs is a pointer to an array of 20 floats. The nulls
     * filter is always designed here and there is no need for
     * caller-designed filters.
     */
    void setNotchX4(float32_t frequency)
       {
       float32_t kf[] = {0.9955f, 0.9982f, 1.0018f, 1.0045f};  // Offset ratios
       float32_t Q = 60.0f;
       float32_t w0, sinW0, alpha, cosW0, scale;
       for (int ii = 0; ii<4; ii++)    // Over 4 cascaded BiQuad filters
          {
          w0 = kf[ii]*frequency*(2.0f*3.141592654f*nDecimate  / sample_rate_Hz);
          sinW0 = sin(w0);
          alpha = sinW0 / (Q * 2.0f);
          cosW0 = cos(w0);
          scale = 1.0f / (1.0f+alpha);
          /* b0 */ iirNullsCoeffs[5*ii + 0] = scale;
          /* b1 */ iirNullsCoeffs[5*ii + 1] = -2.0f * cosW0 * scale;
          /* b2 */ iirNullsCoeffs[5*ii + 2] = scale;
          /* a0 always 1.0f */
          /* a1 */ iirNullsCoeffs[5*ii + 3] = 2.0f * cosW0 * scale;
          /* a2 */ iirNullsCoeffs[5*ii + 4] = -(1.0f - alpha) * scale;
          }
       }
    };
#endif
