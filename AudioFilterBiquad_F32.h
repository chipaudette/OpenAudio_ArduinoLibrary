/*
 * AudioFilterBiquad_F32.h
 * Chip Audette, OpenAudio, Apr 2017
 * MIT License,  Use at your own risk.
 *
 * This filter has been running in F32 as a single stage.  This
 * would work by using multiple instantations, but compute time and
 * latency suffer.  So, Feb 2021 convert to MAX_STAGES 4 as is the I16
 * Teensy Audio library.    Bob Larrkin
 *
 * Float vs Double.  There are times when double precision in the
 * BiQuad calculation is needed to prevent
 * serious numerical errors.  This can be a processor time problem for
 * T3.x.  This routine (NOT QUITE YET) allows for either by
 * a function with float as the default.  This allows different BiQuads
 * to use float or double.  RSL
 *
 * NOTE: If your INO is broken, we had to do it.
 * Some setting of coefficients also did a
 * begin of the ARM CMSIS.  We can't do that with multiple stages. If you
 * encouter this, add myBiquad.begin(); to your INO after the
 * coefficients have been set.  Feb 2021
 *
 * The sign of the coefficients for feedback, the a[], here use the
 * convention of the ARM CMSIS library.  Matlab reverses the signs of these.
 * I believe these are treated per those rules!! Bob
 *
 * Algorithm for CMSIS library
 * Each Biquad stage implements a second order filter using the difference equation:
 *   y[n] = b0 * x[n] + b1 * x[n-1] + b2 * x[n-2] + a1 * y[n-1] + a2 * y[n-2]
 * The a1 and a2 coeccicients do not have minus signs as do the Matlab ones.
 */

#ifndef _filter_iir_f32
#define _filter_iir_f32

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"

// Indicates that the code should just pass through the audio
// without any filtering (as opposed to doing nothing at all)   ,,,,,REMOVE????  WE HAVE DO_BIQUAD THAT DOES THISS
#define IIR_F32_PASSTHRU ((const float32_t *) 1)

// Changed Feb 2021
#define IIR_MAX_STAGES 4

// T4.x can generally use doubles, they may be a burden for T3.x
// Leave commented out to compile for BOTH float and double
// See the function useDouble(bool d) below
// #define NEVER_DOUBLE

class AudioFilterBiquad_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  //GUI: shortName:IIR
  public:
    AudioFilterBiquad_F32(void): AudioStream_F32(1,inputQueueArray) {
        setSampleRate_Hz(AUDIO_SAMPLE_RATE_EXACT);
        sampleRate_Hz = AUDIO_SAMPLE_RATE_EXACT;   //  <<<<<<<<<<<<<<<<<<<<<<  CHECK IF NEEDED??
        doClassInit();
    }
    AudioFilterBiquad_F32(const AudioSettings_F32 &settings):
        AudioStream_F32(1,inputQueueArray) {
            setSampleRate_Hz(settings.sample_rate_Hz);
            doClassInit();
    }

    void doClassInit(void)  {
        for(int ii=0; ii<5*IIR_MAX_STAGES; ii++)  {
           coeff32[ii] = 0.0;
           coeff64[ii] = 0.0;
           }
        for(int ii=0; ii<4; ii++) {
           coeff32[5*ii] = 1.0;  // b0 = 1 for pass through
           coeff64[5*ii] = 1.0;
           }
        numStagesUsed = 0;  // Can be 0 to 4
        doBiquad = false;   // This is the way to jump over the biquad
        }

    // Up to 4 stages are allowed.  Coefficients, either by design function
    // or from direct setCoefficients() need to be added to the double array
    // and also to the float
    void setCoefficients(int iStage, double *cf)  {
        if (iStage > IIR_MAX_STAGES) {
           if (Serial) {
              Serial.print("AudioFilterBiquad_F32: setCoefficients:");
              Serial.println(" *** MaxStages Error");
              }
           return;
           }
       if((iStage + 1) > numStagesUsed)
           numStagesUsed = iStage + 1;  // There may be blank pass throughs
       for(int ii=0; ii<5; ii++)  {
           coeff64[ii + 5*iStage] = cf[ii];  // The local collection of double coefficients
           coeff32[ii + 5*iStage] = (float)cf[ii];  // and of floats
           }
       doBiquad = true;
       }

    // ARM DSP Math library filter instance.
    // Does the initialization of ARM CMSIS DSP BiQuad  structure.  This MUST follow the
    // setting of coefficients to catch the max number of stages and do the
    // double to float conversion for the CMSIS routine.
    void begin(void) {
        // Initialize BiQuad instance (ARM DSP Math Library)
        //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html
        arm_biquad_cascade_df1_init_f32(&iir_inst, numStagesUsed, &coeff32[0],  &StateF32[0]);
        }

    void end(void) {
       doBiquad = false;
       }

    void setSampleRate_Hz(float _fs_Hz) { sampleRate_Hz = _fs_Hz; }

    //  Deprecated
    void setBlockDC(void) {
      // https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html#ga8e73b69a788e681a61bccc8959d823c5
      // Use matlab to compute the coeff for HP at 40Hz: [b,a]=butter(2,40/(44100/2),'high'); %assumes fs_Hz = 44100
      double b[] = {8.173653471988667e-01,    -1.634730694397733e+00,  8.173653471988667e-01};  //from Matlab
      double a[] = { 1.000000000000000e+00,   -1.601092394183619e+00,  6.683689946118476e-01};  //from Matlab
      setFilterCoeff_Matlab(b, a);
    }

    void setFilterCoeff_Matlab(double b[], double a[]) { //one stage of N=2 IIR
      double coeff[5];
      //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html#ga8e73b69a788e681a61bccc8959d823c5
      //Use matlab to compute the coeff, such as: [b,a]=butter(2,20/(44100/2),'high'); %assumes fs_Hz = 44100
      coeff[0] = b[0];   coeff[1] = b[1];  coeff[2] = b[2]; //here are the matlab "b" coefficients
      coeff[3] = -a[1];  coeff[4] = -a[2];  //the DSP needs the "a" terms to have opposite sign vs Matlab
      setCoefficients(0, coeff);
    }

    //Two update() options, floats or doubles
    void useDouble(bool ud)  {
        useDoubleCoefs = ud;  // true is to use doubles
        useDoubleCoefs = false;  //  Not implemented yet
        }

    // Compute common filter functions
    // http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
    //void setLowpass(uint32_t stage, float frequency, float q = 0.7071) {
    void setLowpass(int stage, float frequency, float q) {
        double coeff[5];
        double w0 = frequency * (2 * 3.141592654 / sampleRate_Hz);
        double sinW0 = sin(w0);
        double alpha = sinW0 / ((double)q * 2.0);
        double cosW0 = cos(w0);
        double scale = 1.0 / (1.0+alpha); // which is equal to 1.0 / a0
        /* b0 */ coeff[0] = ((1.0 - cosW0) / 2.0) * scale;
        /* b1 */ coeff[1] = (1.0 - cosW0) * scale;
        /* b2 */ coeff[2] = coeff[0];
        /* a1 */ coeff[3] = -(-2.0 * cosW0) * scale;
        /* a2 */ coeff[4] = -(1.0 - alpha) * scale;
        setCoefficients(stage, coeff);
        }

    void setHighpass(uint32_t stage, float frequency, float q) {
        double coeff[5];
        double w0 = frequency * (2 * 3.141592654 / sampleRate_Hz);
        double sinW0 = sin(w0);
        double alpha = sinW0 / ((double)q * 2.0);
        double cosW0 = cos(w0);
        double scale = 1.0 / (1.0+alpha);
        /* b0 */ coeff[0] = ((1.0 + cosW0) / 2.0) * scale;
        /* b1 */ coeff[1] = -(1.0 + cosW0) * scale;
        /* b2 */ coeff[2] = coeff[0];
        /* a1 */ coeff[3] = -(-2.0 * cosW0) * scale;
        /* a2 */ coeff[4] = -(1.0 - alpha) * scale;
        setCoefficients(stage, coeff);
        }

    void setBandpass(uint32_t stage, float frequency, float q) {
        double coeff[5];
        double w0 = frequency * (2 * 3.141592654 / sampleRate_Hz);
        double sinW0 = sin(w0);
        double alpha = sinW0 / ((double)q * 2.0);
        double cosW0 = cos(w0);
        double scale = 1.0 / (1.0+alpha);
        /* b0 */ coeff[0] = alpha * scale;
        /* b1 */ coeff[1] = 0;
        /* b2 */ coeff[2] = (-alpha) * scale;
        /* a1 */ coeff[3] = -(-2.0 * cosW0) * scale;
        /* a2 */ coeff[4] = -(1.0 - alpha) * scale;
        setCoefficients(stage, coeff);
        }

    // frequency in Hz.  q makes the response stay close to 0.0dB until
    // close to the notch frequency.  q up to 100 or more seem stable.
    void setNotch(uint32_t stage, float frequency, float q) {
        double coeff[5];
        double w0 = frequency * (2 * 3.141592654 / sampleRate_Hz);
        double sinW0 = sin(w0);
        double alpha = sinW0 / ((double)q * 2.0);
        double cosW0 = cos(w0);
        double scale = 1.0 / (1.0+alpha); // which is equal to 1.0 / a0
        /* b0 */ coeff[0] = scale;
        /* b1 */ coeff[1] = (-2.0 * cosW0) * scale;
        /* b2 */ coeff[2] = coeff[0];
        /* a1 */ coeff[3] = -(-2.0 * cosW0) * scale;
        /* a2 */ coeff[4] = -(1.0 - alpha) * scale;
        setCoefficients(stage, coeff);
        }

    void setLowShelf(uint32_t stage, float frequency, float gain, float slope) {
        double coeff[5];
        double a = pow(10.0, gain/40.0);
        double w0 = frequency * (2 * 3.141592654 / sampleRate_Hz);
        double sinW0 = sin(w0);
        //double alpha = (sinW0 * sqrt((a+1/a)*(1/slope-1)+2) ) / 2.0;
        double cosW0 = cos(w0);
        //generate three helper-values (intermediate results):
        double sinsq = sinW0 * sqrt( (pow(a,2.0)+1.0)*(1.0/slope-1.0)+2.0*a );
        double aMinus = (a-1.0)*cosW0;
        double aPlus = (a+1.0)*cosW0;
        double scale = 1.0 / ( (a+1.0) + aMinus + sinsq);
        /* b0 */ coeff[0] =     a * ( (a+1.0) - aMinus + sinsq  ) * scale;
        /* b1 */ coeff[1] =  2.0*a * ( (a-1.0) - aPlus              ) * scale;
        /* b2 */ coeff[2] =     a * ( (a+1.0) - aMinus - sinsq  ) * scale;
        /* a1 */ coeff[3] = 2.0*   ( (a-1.0) + aPlus           ) * scale;
        /* a2 */ coeff[4] =        - ( (a+1.0) + aMinus - sinsq  ) * scale;
        setCoefficients(stage, coeff);
        }

    void setHighShelf(uint32_t stage, float frequency, float gain, float slope) {
        double coeff[5];
        double a = pow(10.0, gain/40.0);
        double w0 = frequency * (2 * 3.141592654 / sampleRate_Hz);
        double sinW0 = sin(w0);
        //double alpha = (sinW0 * sqrt((a+1/a)*(1/slope-1)+2) ) / 2.0;
        double cosW0 = cos(w0);
        //generate three helper-values (intermediate results):
        double sinsq = sinW0 * sqrt( (pow(a,2.0)+1.0)*(1.0/slope-1.0)+2.0*a );
        double aMinus = (a-1.0)*cosW0;
        double aPlus = (a+1.0)*cosW0;
        double scale = 1.0 / ( (a+1.0) - aMinus + sinsq);
        /* b0 */ coeff[0] =     a * ( (a+1.0) + aMinus + sinsq  ) * scale;
        /* b1 */ coeff[1] = -2.0*a * ( (a-1.0) + aPlus              ) * scale;
        /* b2 */ coeff[2] =     a * ( (a+1.0) + aMinus - sinsq  ) * scale;
        /* a1 */ coeff[3] =  -2.0*   ( (a-1.0) - aPlus           ) * scale;
        /* a2 */ coeff[4] =         -( (a+1.0) - aMinus - sinsq  ) * scale;
        setCoefficients(stage, coeff);
    }

    double* getCoeffs(void)  {
        return coeff64;    // Pointer to 20 coefficients in double.
        }

    void update(void);

  private:
    audio_block_f32_t *inputQueueArray[1];
    float  coeff32[5 * IIR_MAX_STAGES];  // Local copies to be transferred with begin()
    double coeff64[5 * IIR_MAX_STAGES];
    float  StateF32[4*IIR_MAX_STAGES];
    //double StateF64[4*IIR_MAX_STAGES];  // Will need this for 64 bit version
    float sampleRate_Hz = AUDIO_SAMPLE_RATE_EXACT; //default.  from AudioStream.h??
    int numStagesUsed = 0;
    bool useDoubleCoefs = false; // As of now, all float <<<<<<<<<<<<<<<<<<<<
    bool doBiquad = false;

    /* Info - The structure from arm_biquad_casd_df1_inst_f32 consists of
     *    uint32_t  numStages;
     *    const float32_t *pCoeffs;  //Points to the array of coefficients, length 5*numStages.
     *    float32_t *pState;         //Points to the array of state variables, length 4*numStages.
     */
    // ARM DSP Math library filter instance.
    arm_biquad_casd_df1_inst_f32 iir_inst;
};

#endif


