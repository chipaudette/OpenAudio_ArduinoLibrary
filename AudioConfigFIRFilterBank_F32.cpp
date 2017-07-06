/*
 * fir_filterbank.cpp
 * 
 * Created: Chip Audette, Creare LLC, Feb 2017
 *   Primarly built upon CHAPRO "Generic Hearing Aid" from 
 *   Boys Town National Research Hospital (BTNRH): https://github.com/BTNRH/chapro
 *   
 * License: MIT License.  Use at your own risk.
 * 
 */

#include "AudioConfigFIRFilterBank_F32.h"
#include "utility/BTNRH_rfft.h"

void AudioConfigFIRFilterBank_F32::fir_filterbank(float *bb, float *cf, const int nc, const int nw_orig, const int wt, const float sr)
    {
        double   p, w, a = 0.16, sm = 0;
        float   *ww, *bk, *xx, *yy;
        int      j, k, kk, nt, nf, ns, *be;

        int nw = nextPowerOfTwo(nw_orig);
        //Serial.print("AudioConfigFIRFilterBank: fir_filterbank: nw_orig = "); Serial.print(nw_orig);
        //Serial.print(", nw = "); Serial.println(nw);
    
        nt = nw * 2;  //we're going to do an fft that's twice as long (zero padded)
        nf = nw + 1;  //number of bins to nyquist in the zero-padded FFT.  Also nf = nt/2+1
        ns = nf * 2;
        be = (int *) calloc(nc + 1, sizeof(int));
        ww = (float *) calloc(nw, sizeof(float));
        xx = (float *) calloc(ns, sizeof(float));
        yy = (float *) calloc(ns, sizeof(float));
        
        // window
        for (j = 0; j < nw; j++) ww[j]=0.0f; //clear
        for (j = 0; j < nw_orig; j++) {
            p = M_PI * (2.0 * j - nw_orig) / nw_orig;
            if (wt == 0) {
                w = 0.54 + 0.46 * cos(p);                   // Hamming
            } else if (wt==1) {
                w = (1 - a + cos(p) + a * cos(2 * p)) / 2;  // Blackman
            } else {
				//win = (1 - cos(2*pi*[1:N]/(N+1)))/2;  //WEA's matlab call, indexing starts from 1, not zero
				w = (1.0 - cosf(2.0*M_PI*((float)(j))/((float)(nw_orig-1))))/2.0; 
			}
            sm += w;
            ww[j] = (float) w;
        }
        
        // frequency bands...add the DC-facing band and add the Nyquist-facing band
        be[0] = 0;
        for (k = 1; k < nc; k++) {
            kk = round(nf * cf[k - 1] * (2 / sr)); //original
            be[k] = (kk > nf) ? nf : kk;
        }
        be[nc] = nf;
        
        // channel tranfer functions
        fzero(xx, ns);
        xx[nw_orig / 2] = 1; //make a single-sample impulse centered on our eventual window
        BTNRH_FFT::cha_fft_rc(xx, nt);
        for (k = 0; k < nc; k++) {
            fzero(yy, ns); //zero the temporary output
            //int nbins = (be[k + 1] - be[k]) * 2;  Serial.print("fir_filterbank: chan ");Serial.print(k); Serial.print(", nbins = ");Serial.println(nbins);
            fcopy(yy + be[k] * 2, xx + be[k] * 2, (be[k + 1] - be[k]) * 2); //copy just our passband
            BTNRH_FFT::cha_fft_cr(yy, nt); //IFFT back into the time domain
            
            // apply window to iFFT of bandpass
            for (j = 0; j < nw; j++) {
                yy[j] *= ww[j];
            }
            
            bk = bb + k * nw_orig; //pointer to location in output array
            fcopy(bk, yy, nw_orig); //copy the filter coefficients to the output array

            //print out the coefficients
            //for (int i=0; i<nw; i++) { Serial.print(yy[i]*1000.0f);Serial.print(" "); }; Serial.println();
        }
        free(be);
        free(ww);
        free(xx);
        free(yy);
    }