/*
 * AudioFilterBiquad_F32
 * 
 * Created: Chip Audette (OpenAudio) Feb 2017
 *
 * License: MIT License.  Use at your own risk.
 * 
 */

#ifndef _filter_iir_f32
#define _filter_iir_f32

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"

// Indicates that the code should just pass through the audio
// without any filtering (as opposed to doing nothing at all)
#define IIR_F32_PASSTHRU ((const float32_t *) 1)

#define IIR_MAX_STAGES 1  //meaningless right now

class AudioFilterBiquad_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  //GUI: shortName:IIR
  public:
    AudioFilterBiquad_F32(void): AudioStream_F32(1,inputQueueArray), coeff_p(IIR_F32_PASSTHRU) { 
		setSampleRate_Hz(AUDIO_SAMPLE_RATE_EXACT); 
	}
	AudioFilterBiquad_F32(const AudioSettings_F32 &settings): 
		AudioStream_F32(1,inputQueueArray), coeff_p(IIR_F32_PASSTHRU) {
			setSampleRate_Hz(settings.sample_rate_Hz); 
	}

    void begin(const float32_t *cp, int n_stages = 1) {
      coeff_p = cp;
      // Initialize FIR instance (ARM DSP Math Library)
      if (coeff_p && (coeff_p != IIR_F32_PASSTHRU) && n_stages <= IIR_MAX_STAGES) {
        //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html
        arm_biquad_cascade_df1_init_f32(&iir_inst, n_stages, (float32_t *)coeff_p,  &StateF32[0]);
      }
    }
    void end(void) {
      coeff_p = NULL;
    }
	
	void setSampleRate_Hz(float _fs_Hz) { sampleRate_Hz = _fs_Hz; }
    
    void setBlockDC(void) {
      //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html#ga8e73b69a788e681a61bccc8959d823c5
      //Use matlab to compute the coeff for HP at 40Hz: [b,a]=butter(2,40/(44100/2),'high'); %assumes fs_Hz = 44100
      float32_t b[] = {8.173653471988667e-01,  -1.634730694397733e+00,   8.173653471988667e-01};  //from Matlab
      float32_t a[] = { 1.000000000000000e+00,   -1.601092394183619e+00,  6.683689946118476e-01};  //from Matlab
      setFilterCoeff_Matlab(b, a);
    }
    
    void setFilterCoeff_Matlab(float32_t b[], float32_t a[]) { //one stage of N=2 IIR
      //https://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html#ga8e73b69a788e681a61bccc8959d823c5
      //Use matlab to compute the coeff, such as: [b,a]=butter(2,20/(44100/2),'high'); %assumes fs_Hz = 44100
      coeff[0] = b[0];   coeff[1] = b[1];  coeff[2] = b[2]; //here are the matlab "b" coefficients
      coeff[3] = -a[1];  coeff[4] = -a[2];  //the DSP needs the "a" terms to have opposite sign vs Matlab ;
      begin(coeff);
    }
	
	//note: stage is currently ignored
	void setCoefficients(int stage, float c[]) {
		if (stage > 0) {
			if (Serial) {
				Serial.println(F("AudioFilterBiquad_F32: setCoefficients: *** ERROR ***"));
				Serial.print(F("    : This module only accepts one stage."));
				Serial.print(F("    : You are attempting to set stage "));Serial.print(stage);
				Serial.print(F("    : Ignoring this filter."));
			}
			return;
		}
		coeff[0] = c[0];
		coeff[1] = c[1];
		coeff[2] = c[2];
		coeff[3] = -c[3];
		coeff[4] = -c[4];
		begin(coeff);
	}
	
		// Compute common filter functions
	// http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
	//void setLowpass(uint32_t stage, float frequency, float q = 0.7071) {
	void setLowpass(uint32_t stage, float frequency, float q = 0.7071) {
		//int coeff[5];
		double w0 = frequency * (2 * 3.141592654 / AUDIO_SAMPLE_RATE_EXACT);
		double sinW0 = sin(w0);
		double alpha = sinW0 / ((double)q * 2.0);
		double cosW0 = cos(w0);
		//double scale = 1073741824.0 / (1.0 + alpha);
		double scale = 1.0 / (1.0+alpha); // which is equal to 1.0 / a0
		/* b0 */ coeff[0] = ((1.0 - cosW0) / 2.0) * scale;
		/* b1 */ coeff[1] = (1.0 - cosW0) * scale;
		/* b2 */ coeff[2] = coeff[0];
		/* a1 */ coeff[3] = (-2.0 * cosW0) * scale;
		/* a2 */ coeff[4] = (1.0 - alpha) * scale;
		
		setCoefficients(stage, coeff);
	}
	void setHighpass(uint32_t stage, float frequency, float q = 0.7071) {
		//int coeff[5];
		double w0 = frequency * (2 * 3.141592654 / AUDIO_SAMPLE_RATE_EXACT);
		double sinW0 = sin(w0);
		double alpha = sinW0 / ((double)q * 2.0);
		double cosW0 = cos(w0);
		double scale = 1.0 / (1.0+alpha); // which is equal to 1.0 / a0
		/* b0 */ coeff[0] = ((1.0 + cosW0) / 2.0) * scale;
		/* b1 */ coeff[1] = -(1.0 + cosW0) * scale;
		/* b2 */ coeff[2] = coeff[0];
		/* a1 */ coeff[3] = (-2.0 * cosW0) * scale;
		/* a2 */ coeff[4] = (1.0 - alpha) * scale;
		setCoefficients(stage, coeff);
	}
	void setBandpass(uint32_t stage, float frequency, float q = 1.0) {
		//int coeff[5];
		double w0 = frequency * (2 * 3.141592654 / AUDIO_SAMPLE_RATE_EXACT);
		double sinW0 = sin(w0);
		double alpha = sinW0 / ((double)q * 2.0);
		double cosW0 = cos(w0);
		double scale = 1.0 / (1.0+alpha); // which is equal to 1.0 / a0
		/* b0 */ coeff[0] = alpha * scale;
		/* b1 */ coeff[1] = 0;
		/* b2 */ coeff[2] = (-alpha) * scale;
		/* a1 */ coeff[3] = (-2.0 * cosW0) * scale;
		/* a2 */ coeff[4] = (1.0 - alpha) * scale;
		setCoefficients(stage, coeff);
	}
	void setNotch(uint32_t stage, float frequency, float q = 1.0) {
		//int coeff[5];
		double w0 = frequency * (2 * 3.141592654 / AUDIO_SAMPLE_RATE_EXACT);
		double sinW0 = sin(w0);
		double alpha = sinW0 / ((double)q * 2.0);
		double cosW0 = cos(w0);
		double scale = 1.0 / (1.0+alpha); // which is equal to 1.0 / a0
		/* b0 */ coeff[0] = scale;
		/* b1 */ coeff[1] = (-2.0 * cosW0) * scale;
		/* b2 */ coeff[2] = coeff[0];
		/* a1 */ coeff[3] = (-2.0 * cosW0) * scale;
		/* a2 */ coeff[4] = (1.0 - alpha) * scale;
		setCoefficients(stage, coeff);
	}
	void setLowShelf(uint32_t stage, float frequency, float gain, float slope = 1.0f) {
		//int coeff[5];
		double a = pow(10.0, gain/40.0);
		double w0 = frequency * (2 * 3.141592654 / AUDIO_SAMPLE_RATE_EXACT);
		double sinW0 = sin(w0);
		//double alpha = (sinW0 * sqrt((a+1/a)*(1/slope-1)+2) ) / 2.0;
		double cosW0 = cos(w0);
		//generate three helper-values (intermediate results):
		double sinsq = sinW0 * sqrt( (pow(a,2.0)+1.0)*(1.0/slope-1.0)+2.0*a );
		double aMinus = (a-1.0)*cosW0;
		double aPlus = (a+1.0)*cosW0;
		double scale = 1.0 / ( (a+1.0) + aMinus + sinsq);
		/* b0 */ coeff[0] =		a *	( (a+1.0) - aMinus + sinsq	) * scale;
		/* b1 */ coeff[1] =  2.0*a * ( (a-1.0) - aPlus  			) * scale;
		/* b2 */ coeff[2] =		a * ( (a+1.0) - aMinus - sinsq 	) * scale;
		/* a1 */ coeff[3] = -2.0*	( (a-1.0) + aPlus			) * scale;
		/* a2 */ coeff[4] =  		( (a+1.0) + aMinus - sinsq	) * scale;
		setCoefficients(stage, coeff);
	}
	void setHighShelf(uint32_t stage, float frequency, float gain, float slope = 1.0f) {
		//int coeff[5];
		double a = pow(10.0, gain/40.0);
		double w0 = frequency * (2 * 3.141592654 / AUDIO_SAMPLE_RATE_EXACT);
		double sinW0 = sin(w0);
		//double alpha = (sinW0 * sqrt((a+1/a)*(1/slope-1)+2) ) / 2.0;
		double cosW0 = cos(w0);
		//generate three helper-values (intermediate results):
		double sinsq = sinW0 * sqrt( (pow(a,2.0)+1.0)*(1.0/slope-1.0)+2.0*a );
		double aMinus = (a-1.0)*cosW0;
		double aPlus = (a+1.0)*cosW0;
		double scale = 1.0 / ( (a+1.0) - aMinus + sinsq);
		/* b0 */ coeff[0] =		a *	( (a+1.0) + aMinus + sinsq	) * scale;
		/* b1 */ coeff[1] = -2.0*a * ( (a-1.0) + aPlus  			) * scale;
		/* b2 */ coeff[2] =		a * ( (a+1.0) + aMinus - sinsq 	) * scale;
		/* a1 */ coeff[3] =  2.0*	( (a-1.0) - aPlus			) * scale;
		/* a2 */ coeff[4] =  		( (a+1.0) - aMinus - sinsq	) * scale;
		setCoefficients(stage, coeff);
	}
    
    void update(void);
   
  private:
    audio_block_f32_t *inputQueueArray[1];
    float32_t coeff[5 * 1] = {1.0, 0.0, 0.0, 0.0, 0.0}; //no filtering. actual filter coeff set later
	float sampleRate_Hz = AUDIO_SAMPLE_RATE_EXACT; //default.  from AudioStream.h??
  
    // pointer to current coefficients or NULL or FIR_PASSTHRU
    const float32_t *coeff_p;
  
    // ARM DSP Math library filter instance
    arm_biquad_casd_df1_inst_f32 iir_inst;
    float32_t StateF32[4*IIR_MAX_STAGES];
};



#endif


