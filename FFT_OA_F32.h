/*
 * FFT_F32
 * 
 * Purpose: Encapsulate the ARM floating point FFT/IFFT functions
 *          in a way that naturally handles the radix2 vs radix4
 *          constraints on FFT size inherent to the particular
 *          version of the ARM CMSIS FFT functions included with
 *          the Teensy libraries.
 * 
 * Created: Chip Audette (openaudio.blogspot.com)
 *          Jan-Jul 2017
 * 
 * License: MIT License
 */

#ifndef _FFT_OA_h
#define _FFT_OA_h

#include <Arduino.h>  //for Serial
//include <math.h>
#include <arm_math.h>

class FFT_F32
{
  public:
    FFT_F32(void) {};
    FFT_F32(const int _N_FFT) {
      setup(_N_FFT);
    }
    FFT_F32(const int _N_FFT, const int _is_IFFT) {
      setup(_N_FFT, _is_IFFT);
    }
    ~FFT_F32(void) { delete window; };  //destructor

    virtual int setup(const int _N_FFT) {
      int _is_IFFT = 0;
      return setup(_N_FFT,_is_IFFT);
    }
    virtual int setup(const int _N_FFT, const int _is_IFFT) {
      if (!is_valid_N_FFT(_N_FFT)) {
        Serial.println(F("FFT_F32: *** ERROR ***"));
        Serial.print(F("    : Cannot use N_FFT = ")); Serial.println(N_FFT);
        Serial.print(F("    : Must be power of 2 between 16 and 2048"));
        return -1;
      }
      N_FFT = _N_FFT;
      is_IFFT = _is_IFFT;

      if ((N_FFT == 16) || (N_FFT == 64) || (N_FFT == 256) || (N_FFT == 1024)) {
        arm_cfft_radix4_init_f32(&fft_inst_r4, N_FFT, is_IFFT, 1); //FFT
        is_rad4 = 1;
      } else {
        arm_cfft_radix2_init_f32(&fft_inst_r2, N_FFT, is_IFFT, 1); //FFT
      }

      //allocate window
      window = new float[N_FFT];
      if (is_IFFT) {
        useRectangularWindow(); //default to no windowing for IFFT
      } else {
        useHanningWindow(); //default to windowing for FFT
      }
      return N_FFT;
    }
    static int is_valid_N_FFT(const int N) {
       if ((N == 16) || (N == 32) || (N == 64) || (N == 128) || 
        (N == 256) || (N == 512) || (N==1024) || (N==2048)) {
          return 1;
        } else {
          return 0;
        }
    }

    virtual void useRectangularWindow(void) {
      flag__useWindow = 0;
      if (window != NULL) {
        for (int i=0; i < N_FFT; i++) window[i] = 1.0; 
      }
      //if (Serial) { Serial.print("FFT_F32: useRectangularWindow.  flag__useWindow = "); Serial.println(flag__useWindow); }
    }
    virtual void useHanningWindow(void) {
      flag__useWindow = 1;
      if (window != NULL) {
        //Serial.print("FFT_F32: useHanningWindow, N=");
		//Serial.print(N_FFT); Serial.print(": ");
        for (int i=0; i < N_FFT; i++) { 
          window[i] = 0.5*(1.0 - cosf(2.0*M_PI*(float)i/((float)(N_FFT-1))));
          //Serial.print(window[i]);
          //Serial.print(" ");
        }
        //Serial.println();
      }
      //if (Serial) { Serial.print("FFT_F32: useHanningWindow.  flag__useWindow = "); Serial.println(flag__useWindow); }
    }
    
    virtual void applyWindowToRealPartOfComplexVector(float32_t *complex_2N_buffer) {
      for (int i=0; i < N_FFT; i++) {
        complex_2N_buffer[2*i] *= window[i];
      }
    }
    virtual void applyWindowToRealVector(float32_t *real_N_buffer) {
      for (int i=0; i < N_FFT; i++) {
        real_N_buffer[i] *= window[i];
      }
    }

    virtual void execute(float32_t *complex_2N_buffer) { //interleaved [real,imaginary], total length is 2*N_FFT
      if (N_FFT == 0) return;

      //apply window before FFT (if it is an FFT and not IFFT)
      if ((!is_IFFT) && (flag__useWindow)) applyWindowToRealPartOfComplexVector(complex_2N_buffer);
	  
      //do the FFT
      if (is_rad4) {
        arm_cfft_radix4_f32(&fft_inst_r4, complex_2N_buffer);
      } else {
        arm_cfft_radix2_f32(&fft_inst_r2, complex_2N_buffer);
      }

      //apply window after FFT (if it is an IFFT and not FFT)
      if ((is_IFFT) && (flag__useWindow)) applyWindowToRealPartOfComplexVector(complex_2N_buffer);

	}
    
    virtual void rebuildNegativeFrequencySpace(float *complex_2N_buffer) {
      //create the negative frequency space via complex conjugate of the positive frequency space

	  //int ind_nyquist_bin = N_FFT/2;  //nyquist is neither positive nor negative
	  //int targ_ind = ind_nyquist_bin+1; //negative frequencies start start one above nyquist
	  //for (int source_ind = ind_nyquist_bin-1; source_ind > 0; source_ind--) {  //exclude the 0'th bin as DC is neither positive nor negative
		//complex_2N_buffer[2*targ_ind] = complex_2N_buffer[2*source_ind]; //real
		//complex_2N_buffer[2*targ_ind+1] = -complex_2N_buffer[2*source_ind+1]; //imaginary.  negative makes it the complex conjugate, which is what we want for the neg freq space
		//targ_ind++;
	  //}

	  int targ_ind = 0;
	  for (int source_ind = 1; source_ind < (N_FFT/2-1); source_ind++) {
			targ_ind = N_FFT - source_ind;
			complex_2N_buffer[2*targ_ind] = complex_2N_buffer[2*source_ind]; //real
			complex_2N_buffer[2*targ_ind+1] = -complex_2N_buffer[2*source_ind+1]; //imaginary.  negative makes it the complex conjugate, which is what we want for the neg freq space
	  }

    }
    virtual int getNFFT(void) { return N_FFT; };
    int get_flagUseWindow(void) { return flag__useWindow; };

  private:
    int N_FFT=0;
    int is_IFFT=0;
    int is_rad4=0;
    float *window;
    int flag__useWindow=0;
    arm_cfft_radix4_instance_f32 fft_inst_r4;
    arm_cfft_radix2_instance_f32 fft_inst_r2;
     
};


class IFFT_F32 : public FFT_F32  // is basically the same as FFT, so let's inherent FFT
{
  public:
    IFFT_F32(void) : FFT_F32() {};
    IFFT_F32(const int _N_FFT): FFT_F32(_N_FFT) {
      //constructor
      IFFT_F32::setup(_N_FFT); //call FFT's setup routine
    }
    virtual int setup(const int _N_FFT) {
      const int _is_IFFT = 1;
      return FFT_F32::setup(_N_FFT, _is_IFFT); //call FFT's setup routine      
    }
    //all other functions are in FFT
};

#endif
