/* AudioFilterFIRGeneralr_F32.cpp
 *
 * Bob Larkin,  W7PUA  20 May 2020 (c)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
 /* A math note - The design of the FIR filter from the frequency response requires
  * taking a discrete Fourier transform.  The calculation of the achieved
  * frequency response requires another Fourier transform.  Good news is
  * that this is arithmetically simple, because of
  * working with real (not complex) inputs and requiring the FIR be linear
  * phase.  One of a number of references is C.S. Burris and T. W. Parks,
  * "Digital Filter Design."  That book also has excellent sections on
  * Chebychev error (Parks-McClellan-Rabiner) FIR filters and IIR Filters.
  */

#include "AudioFilterFIRGeneral_F32.h"

void AudioFilterFIRGeneral_F32::update(void)  {
    audio_block_f32_t *blockIn, *blockOut;

#if TEST_TIME_FIRG
  if (iitt++ >1000000) iitt = -10;
  uint32_t t1, t2;
  t1 = tElapse;
#endif

    blockIn = AudioStream_F32::receiveReadOnly_f32();
    if (!blockIn) return;

    // If there's no coefficient table, give up.
    if (cf32f == NULL) {
      AudioStream_F32::release(blockIn);
      return;
    }

    blockOut = AudioStream_F32::allocate_f32();  // get a block for the FIR output
    if (blockOut) {
        // The FIR update
        arm_fir_f32(&fir_inst, blockIn->data, blockOut->data, blockIn->length);
        AudioStream_F32::transmit(blockOut); // send the FIR output
        AudioStream_F32::release(blockOut);
    }
    AudioStream_F32::release(blockIn);

#if TEST_TIME_FIRG
  t2 = tElapse;
  if(iitt++ < 0) {Serial.print("At FIRGeneral end, microseconds = "); Serial.println (t2 - t1); //           }
	Serial.print("numtaps = "); Serial.println (fir_inst.numTaps); 
	}
  t1 = tElapse;
#endif
}

/* FIRGeneralNew() calculates the generalFIR filter coefficients. Works from:
 *   adb         Pointer to nFIR/2 array adb[] of levels, in dB
 *   nFIR        The number of FIR coefficients (taps) used
 *   cf32f       Pointer to an array of float to hold nFIR FIR coefficients
 *   kdb         A parameter that trades off sidelobe levels for sharpness of band transition.
 *               kdb=30 sharp cutoff, poor sidelobes
 *               kdb=60 slow cutoff, low sidelobes
 *   pStateArray Pointer to 128+nFIR array of floats, used here briefly and then
 *               passed to the FIR routine as working storage
 *
 * The arrays, adb[], cf32f[] and pStateArray[] are supplied by the calling .INO
 *
 * Returns: 0 if successful, or an error code if not.
 * Errors:  1 = NU
 *          2 = sidelobe level out of range, must be > 0
 *          3 = nFIR out of range
 *
 * Note - This function runs at setup time, so slowness is not an issue
 */
uint16_t AudioFilterFIRGeneral_F32::FIRGeneralNew(
         float32_t *adb, uint16_t _nFIR, float32_t *_cf32f, float32_t kdb,
         float32_t *pStateArray) {

    uint16_t i, k, n;
    uint16_t nHalfFIR;
    float32_t beta, kbes;
    float32_t num, xn2, scaleXn2, WindowWt;
    float32_t M;                // Burris and Parks, (2.16)
    mathDSP_F32 mathEqualizer;  // For Bessel function
    bool even;

    cf32f = _cf32f;  // Make pivate copies
    nFIR = _nFIR;

    // Check range of nFIR
    if (nFIR<4)
        return ERR_FIRGEN_NFIR;
        
    M = 0.5f*(float32_t)(nFIR - 1);
    // The number of FIR coefficients is even or odd
    if (2*(nFIR/2) == nFIR) {
        even = true; 
        nHalfFIR = nFIR/2;
     }
     else {
        even = false; 
        nHalfFIR = (nFIR - 1)/2;
      }

    for (i=0; i<nFIR; i++)  // To be sure, zero the coefficients
       cf32f[i] = 0.0f;
    for(i=0; i<(nFIR+AUDIO_BLOCK_SAMPLES); i++)  // And the storage
       pStateArray[i] = 0.0f;

    // Convert dB to "Voltage" ratios, frequencies to fractions of sampling freq
    // Borrow pStateArray, as it has not yet gone to ARM math
    for(i=0; i<=nHalfFIR; i++)
        pStateArray[i] = powf(10.0, 0.05f*adb[i]);

    /* Find FIR coefficients, the Fourier transform of the frequency
     * response. This general frequency description has half as many
     * frequency dB points as FIR coefficients.  If nFIR is odd,
     * the number of frequency points is nHalfFIR = (nFIR - 1)/2.
     *
     * Numbering example for nFIR==21:
     * Odd nFIR:  Subscript 0 to 9 is 10 taps;  11 to 20 is 10 taps; 10+1+10=21 taps
     *
     * Numbering example for nFIR==20: 
     * Even nFIR: Subscript 0 to 9 is 10 taps;  10 to 19 is 10 taps; 10+10=20 taps
     */
    float32_t oneOnNFIR = 1.0f / (float32_t)nFIR;
    if(even) {
       for(n=0; n<nHalfFIR; n++) {    // Over half of even nFIR FIR coeffs	  
           cf32f[n] = pStateArray[0];   // DC term
           for(k=1; k<nHalfFIR;  k++) {  // Over all frequencies, half of nFIR
               num = (M - (float32_t)n) * (float32_t)k; 
               cf32f[n] +=  2.0f*pStateArray[k]*cosf(MF_TWOPI*num*oneOnNFIR);
            }
            cf32f[n] *= oneOnNFIR;  // Divide by nFIR
       }				 
    }
    else {    // nFIR is odd		 
       for(n=0; n<=nHalfFIR; n++) {    // Over half of FIR coeffs, nFIR odd
           cf32f[n] = pStateArray[0];
           for(k=1; k<=nHalfFIR;  k++) {
               num = (float32_t)((nHalfFIR - n)*k);
               cf32f[n] +=  2.0f*pStateArray[k]*cosf(MF_TWOPI*num*oneOnNFIR);
            }
            cf32f[n] *= oneOnNFIR;
       }     
	}
    /* At this point, the cf32f[] coefficients are simply truncated, creating
     * high sidelobe responses. To reduce the sidelobes, a windowing function is applied.
     * This has the side affect of increasing the rate of cutoff for sharp frequency transition.
     * The only windowing function available here is that of James Kaiser.  This has a number
     * of desirable features. The sidelobes drop off as the frequency away from a transition.
     * Also, the tradeoff of sidelobe level versus cutoff rate is variable.
     * Here we specify it in terms of kdb, the highest sidelobe, in dB, next to a sharp cutoff. For
     * calculating the windowing vector, we need a parameter beta, found as follows:
     */
    if (kdb<0.0f) return ERR_FIRGEN_SIDELOBES;
    if (kdb < 20.0f)
        beta = 0.0;
    else
        beta = -2.17+0.17153*kdb-0.0002841*kdb*kdb; // Within a dB or so
    // Note: i0f is the fp zero'th order modified Bessel function (see mathDSP_F32.h)
    kbes = 1.0f / mathEqualizer.i0f(beta);      // An additional derived parameter used in loop
	scaleXn2 = 4.0f / ( ((float32_t)nFIR - 1.0f)*((float32_t)nFIR - 1.0f) ); // Needed for even & odd
    if (even) {
		// nHalfFIR = nFIR/2;   for nFIR even
        for (n=0; n<nHalfFIR; n++) {  // For 20 Taps, this is 0 to 9
            xn2 = 0.5f+(float32_t)n;
            xn2 = scaleXn2*xn2*xn2;
            WindowWt=kbes*(mathEqualizer.i0f(beta*sqrt(1.0-xn2)));
            if(kdb > 0.1f)                                 // kdb==0.0 means no window
                cf32f[nHalfFIR - n - 1] *= WindowWt;       // Apply window, reverse subscripts
            cf32f[nHalfFIR + n] = cf32f[nHalfFIR - n - 1]; // and create the upper half
        }
	}
	else {   // nFIR is odd,   nHalfFIR = (nFIR - 1)/2
        for (n=0; n<=nHalfFIR; n++) {  // For 21 Taps, this is 0 to 10, including center
            xn2 = (int16_t)(0.5f+(float32_t)n);
            xn2 = scaleXn2*xn2*xn2;
            WindowWt=kbes*(mathEqualizer.i0f(beta*sqrt(1.0-xn2)));
            if(kdb > 0.1f)
                cf32f[nHalfFIR - n] *= WindowWt;
            // 21 taps, for n=0, nHalfFIR-n = 10,  for n=1 nHalfFIR-n=9,  for n=nHalfFIR, nHalfFIR-n=0
            cf32f[nHalfFIR + n] = cf32f[nHalfFIR - n]; // and create upper half (rewrite center is OK)
        }
    }
    // And finally, fill in the members of fir_inst given in update() to the ARM FIR routine.
    AudioNoInterrupts();
    arm_fir_init_f32(&fir_inst, nFIR, (float32_t *)cf32f, &pStateArray[0], (uint32_t)block_size);
    AudioInterrupts(); 
    return 0;
}

// FIRGeneralLoad() allows an array of nFIR FIR coefficients to be loaded.  They come from an .INO
// supplied array.  Also, pStateArray[] is .INO supplied and must be (block_size + nFIR) in size.
uint16_t AudioFilterFIRGeneral_F32::LoadCoeffs(uint16_t _nFIR, float32_t *_cf32f, float32_t *pStateArray) {
    nFIR = _nFIR;
    cf32f = _cf32f;
    if (nFIR<4)         // Check range of nFIR
        return ERR_FIRGEN_NFIR;
    for(int i=0; i<(nFIR+AUDIO_BLOCK_SAMPLES); i++)  // Zero, to be sure
        pStateArray[i] = 0.0f;
    AudioNoInterrupts(); 
    arm_fir_init_f32(&fir_inst, nFIR, &cf32f[0], &pStateArray[0], (uint32_t)block_size);
    AudioInterrupts(); 
    return 0;
}

/* Calculate frequency response in dB.  Leave nFreq point result in array rdb[] supplied
 * by the calling .INO  See B&P p27 (Type 1 and 2). Be aware that if nFIR*nFreq is big,
 * like 100,000 or more, this will take a while to calcuulate all the cosf().  Normally,
 * this is not an issue as this is an infrequent calculation.
 * This function assumes that the phase of the FIR is linear with frequency, i.e.,
 * the coefficients are symmetrical about the middle.  Otherwise, doubling of values,
 * as  is done here, is not valid.
 */
void AudioFilterFIRGeneral_F32::getResponse(uint16_t nFreq, float32_t *rdb)  {
    uint16_t i, n;
    float32_t bt;
    float32_t piOnNfreq;
    uint16_t nHalfFIR;
    float32_t M;

    piOnNfreq = MF_PI / (float32_t)nFreq;
    // The number of FIR coefficients, even or odd?
    if (2*(nFIR/2) == nFIR) {     // it is even
		nHalfFIR = nFIR/2;
        M = 0.5f*(float32_t)(nFIR - 1);
        for (i=0; i<nFreq; i++) {
            bt = 0.0;
            for (n=0; n<nHalfFIR; n++)  // Add in terms twice, as they are symmetric
                 bt += 2.0f*cf32f[n]*cosf(piOnNfreq*((M-(float32_t)n)*(float32_t)i));
            rdb[i] = 20.0f*log10f(fabsf(bt));     // Convert to dB
        }
    }
    else {                         // it is odd
        nHalfFIR = (nFIR - 1)/2;
        for (i=0; i<nFreq; i++) {
            bt = cf32f[nHalfFIR];       // Center coefficient
            for (n=0; n<nHalfFIR; n++)  // Add in the others twice, as they are symmetric
                bt += 2.0f*cf32f[n]*cosf(piOnNfreq*(float32_t)((nHalfFIR-n)*i));
            rdb[i] = 20.0f*log10f(fabsf(bt));
        }
    }      
}
