/*
 * AudioCalcEnvelope_F32
 * 
 * Created: Chip Audette, Feb 2017
 * Purpose: This module extracts the envelope of the audio signal.
 * Derived From: Core envelope extraction algorithm is from "smooth_env"
 *     WDRC_circuit from CHAPRO from BTNRC: https://github.com/BTNRH/chapro
 *     As of Feb 2017, CHAPRO license is listed as "Creative Commons?"
 *          
 * This processes a single stream fo audio data (ie, it is mono)       
 *          
 * MIT License.  use at your own risk.
*/

#ifndef _AudioCalcEnvelope_F32_h
#define _AudioCalcEnvelope_F32_h

#include <arm_math.h> //ARM DSP extensions.  for speed!
#include <AudioStream_F32.h>

class AudioCalcEnvelope_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  //GUI: shortName:calc_envelope
  public:
    //default constructor
    AudioCalcEnvelope_F32(void) : AudioStream_F32(1, inputQueueArray_f32),
		sample_rate_Hz(AUDIO_SAMPLE_RATE) { setDefaultValues(); };
	AudioCalcEnvelope_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32),
		sample_rate_Hz(settings.sample_rate_Hz) { setDefaultValues(); };

    //here's the method that does all the work
    void update(void) {
		
		//get the input audio data block
		audio_block_f32_t *in_block = AudioStream_F32::receiveReadOnly_f32();
		if (!in_block) return;
		
		//check format
		if (in_block->fs_Hz != sample_rate_Hz) {
			Serial.println("AudioComputeEnvelope_F32: *** WARNING ***: Data sample rate does not match expected.");
			Serial.println("AudioComputeEnvelope_F32: Changing sample rate.");
			setSampleRate_Hz(in_block->fs_Hz);
		}

		//prepare an output data block
		audio_block_f32_t *out_block = AudioStream_F32::allocate_f32();
		if (!out_block) return;
		
		// /////////// put the actual processing here
		smooth_env(in_block->data, out_block->data, in_block->length);
		out_block->length = in_block->length; out_block->fs_Hz = in_block->fs_Hz;
		
		//transmit the block and be done
		AudioStream_F32::transmit(out_block);
		AudioStream_F32::release(out_block);
		AudioStream_F32::release(in_block);
		
    }
	
	//compute the smoothed signal envelope
	//compute the envelope of the signal, not of the signal power)
	void smooth_env(float x[], float y[], const int n) {
        float  xab, xpk;
        int k;
    
        // find envelope of x and return as y
        //xpk = *ppk;                     // start with previous xpk
		xpk = state_ppk;
        for (k = 0; k < n; k++) {
          xab = (x[k] >= 0.0f) ? x[k] : -x[k];
          if (xab >= xpk) {
              xpk = alfa * xpk + (1.f-alfa) * xab;
          } else {
              xpk = beta * xpk;
          }
          y[k] = xpk;
        }
        //*ppk = xpk;                     // save xpk for next time
		state_ppk = xpk;
	}
	
	//convert time constants from seconds to unitless parameters, from CHAPRO, agc_prepare.c
	void setAttackRelease_msec(const float atk_msec, const float rel_msec) {
		given_attack_msec = atk_msec;
		given_release_msec = rel_msec;
		
		// convert ANSI attack & release times to filter time constants
        float ansi_atk = 0.001f * atk_msec * sample_rate_Hz / 2.425f; 
        float ansi_rel = 0.001f * rel_msec * sample_rate_Hz / 1.782f; 
        alfa = (float) (ansi_atk / (1.0f + ansi_atk));
        beta = (float) (ansi_rel / (10.f + ansi_rel));
	}

	void setDefaultValues(void) {
		float32_t attack_msec = 5.0f;
		float32_t release_msec = 50.0f;
		setAttackRelease_msec(attack_msec, release_msec);
		state_ppk = 0; //initialize
	}

    void setSampleRate_Hz(const float &fs_Hz) {
		//change params that follow sample rate
		
		sample_rate_Hz = fs_Hz;
	}
	
	void resetStates(void) { state_ppk = 1.0; }
	float getCurrentLevel(void) { return state_ppk; } 
  private:
    audio_block_f32_t *inputQueueArray_f32[1]; //memory pointer for the input to this module
	float32_t sample_rate_Hz;
	float32_t given_attack_msec, given_release_msec;
	float32_t alfa, beta;  //time constants, but in terms of samples, not seconds
	float32_t state_ppk = 1.0f;
};

#endif