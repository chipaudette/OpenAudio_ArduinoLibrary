/*
 * AudioFilterFIR_F32
 * 
 * Created: Chip Audette (OpenAudio) Feb 2017
 *    - Building from AudioFilterFIR from Teensy Audio Library (AudioFilterFIR credited to Pete (El Supremo))
 * 
 */

#ifndef _filter_fir_f32_h
#define _filter_fir_f32_h

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"

// Indicates that the code should just pass through the audio
// without any filtering (as opposed to doing nothing at all)
#define FIR_F32_PASSTHRU ((const float32_t *) 1)
#define FIR_MAX_COEFFS 200

class AudioFilterFIR_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node  
	public:
		AudioFilterFIR_F32(void): AudioStream_F32(1,inputQueueArray), 
			coeff_p(FIR_F32_PASSTHRU), n_coeffs(1), configured_block_size(0) {	}
		AudioFilterFIR_F32(const AudioSettings_F32 &settings): AudioStream_F32(1,inputQueueArray), 
			coeff_p(FIR_F32_PASSTHRU), n_coeffs(1), configured_block_size(0) {	}
			
		//initialize the FIR filter by giving it the filter coefficients
		void begin(const float32_t *cp, const int _n_coeffs) { begin(cp, _n_coeffs, AUDIO_BLOCK_SAMPLES); } //assume that the block size is the maximum
		void begin(const float32_t *cp, const int _n_coeffs, const int block_size) {  //or, you can provide it with the block size
			coeff_p = cp;
			n_coeffs = _n_coeffs;
			
			// Initialize FIR instance (ARM DSP Math Library)
			if (coeff_p && (coeff_p != FIR_F32_PASSTHRU) && n_coeffs <= FIR_MAX_COEFFS) {
				arm_fir_init_f32(&fir_inst, n_coeffs, (float32_t *)coeff_p,  &StateF32[0], block_size);
				configured_block_size = block_size;
				//Serial.print("AudioFilterFIR_F32: FIR is initialized. N_FIR = "); Serial.print(n_coeffs);
				//Serial.print(", Block Size = "); Serial.println(block_size);
			//} else {
			//	Serial.print("AudioFilterFIR_F32: *** ERROR ***: Cound not initialize. N_FIR = "); Serial.print(n_coeffs);
			//	Serial.print(", Block Size = "); Serial.println(block_size);
			//	coeff_p = NULL;
			}
		}
		void end(void) {  coeff_p = NULL; }
		void update(void);

		//void setBlockDC(void) {}	//helper function that sets this up for a first-order HP filter at 20Hz
		
	private:
		audio_block_f32_t *inputQueueArray[1];

		// pointer to current coefficients or NULL or FIR_PASSTHRU
		const float32_t *coeff_p;
		int n_coeffs;
		int configured_block_size;

		// ARM DSP Math library filter instance
		arm_fir_instance_f32 fir_inst;
		float32_t StateF32[AUDIO_BLOCK_SAMPLES + FIR_MAX_COEFFS];
};


#endif


