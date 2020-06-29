/* AudioFilterEqualizer_F32.cpp
 *
 * Bob Larkin,  W7PUA  8 May 2020
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

#include "AudioFilterEqualizer_F32.h"

void AudioFilterEqualizer_F32::update(void)  {
    audio_block_f32_t *block, *block_new;

#if TEST_TIME_EQ
  if (iitt++ >1000000) iitt = -10;
  uint32_t t1, t2; 
  t1 = tElapse;
#endif

    block = AudioStream_F32::receiveReadOnly_f32();
    if (!block) return;

    // If there's no coefficient table, give up.  
    if (cf32f == NULL) {
      AudioStream_F32::release(block);
      return;
    }

	block_new = AudioStream_F32::allocate_f32(); 	// get a block for the FIR output
	if (block_new) {
		//apply the FIR
		arm_fir_f32(&fir_inst, block->data, block_new->data, block->length);
		AudioStream_F32::transmit(block_new); // send the FIR output
		AudioStream_F32::release(block_new);
	}
	AudioStream_F32::release(block);
	
#if TEST_TIME_EQ   
  t2 = tElapse;
  if(iitt++ < 0) {Serial.print("At AnalyzePhase end, microseconds = ");  Serial.println (t2 - t1); }  
  t1 = tElapse;
#endif   
}

/* equalizerNew() calculates the Equalizer FIR filter coefficients. Works from: 
 * uint16_t equalizerNew(uint16_t _nBands, float32_t *feq, float32_t *adb,
                      uint16_t _nFIR, float32_t *_cf32f, float32_t kdb)
 *   nBands   Number of equalizer bands
 *   feq      Pointer to array feq[] of nBands breakpoint frequencies, fractions of sample rate, Hz
 *   adb      Pointer to array aeq[] of nBands levels, in dB, for the feq[] defined frequency bands
 *   nFIR     The number of FIR coefficients (taps) used in the equalzer
 *   cf32f    Pointer to an array of float to hold FIR coefficients
 *   kdb      A parameter that trades off sidelobe levels for sharpness of band transition.
 *            kdb=30 sharp cutoff, poor sidelobes
 *            kdb=60 slow cutoff, low sidelobes
 * 
 * The arrays, feq[], aeq[] and cf32f[] are supplied by the calling .INO
 *
 * Returns: 0 if successful, or an error code if not.
 * Errors:  1 = Too many bands, 50 max
 *          2 = sidelobe level out of range, must be > 0
 *          3 = nFIR out of range
 *
 * Note - This function runs at setup time, and there is no need to fret about
 * processor speed.  Likewise, local arrays are created on the stack and are
 * available for other use when this function closes.
 */
uint16_t AudioFilterEqualizer_F32::equalizerNew(uint16_t _nBands, float32_t *feq, float32_t *adb,
                      uint16_t _nFIR, float32_t *_cf32f, float32_t kdb)  {
    uint16_t i, j;
    uint16_t nHalfFIR;
    float32_t beta, kbes;
    float32_t q, xj2, scaleXj2, WindowWt;
    float32_t fNorm[50];   // Normalized to the sampling frequency
    float32_t aVolts[50];  // Convert from dB to "quasi-Volts"
    mathDSP_F32 mathEqualizer;  // For Bessel function

    // Make private copies
    cf32f = _cf32f;
    nFIR = _nFIR;
    nBands = _nBands;

    // Check range of nFIR
    if (nFIR<5 || nFIR>EQUALIZER_MAX_COEFFS)
        return ERR_EQ_NFIR;

    // The number of FIR coefficients needs to be odd
    if (2*(nFIR/2) == nFIR)
        nFIR -= 1;  // We just won't use the last element of the array
    nHalfFIR = (nFIR - 1)/2;  // If nFIR=199, nHalfFIR=99
 
    for (int kk = 0; kk<nFIR; kk++)  // To be sure, zero the coefficients
      cf32f[kk] = 0.0f;
      
    // Convert dB to Voltage ratios, frequencies to fractions of sampling freq
    if(nBands <2 || nBands>50)  return ERR_EQ_BANDS;
    for (i=0; i<nBands; i++)  {
       aVolts[i]=powf(10.0, (0.05*adb[i]));
       fNorm[i]=feq[i]/sample_rate_Hz;
    }

    /* Find FIR coefficients, the Fourier transform of the frequency
     * response. This is done by dividing the response into a sequence
     * of nBands rectangular frequency blocks, each of a different level.
     * We can precalculate the Fourier transform for each rectangular band.
     * The linearity of the Fourier transform allows us to sum the transforms
     * of the individual blocks to get pre-windowed coefficients.  As follows
     *
     * Numbering example for nFIR==199:
     * Subscript 0 to 98 is 99 taps;  100 to 198 is 99 taps;  99+1+99=199 taps
     * The center coef ( for nFIR=199 taps, nHalfFIR=99 ) is a 
     * special case that comes from sin(0)/0 and treated first:
     */
    cf32f[nHalfFIR] = 2.0f*(aVolts[0]*fNorm[0]);  // Coefficient "99"
    for(i=1; i<nBands; i++) {
       cf32f[nHalfFIR] += 2.0f*aVolts[i]*(fNorm[i]-fNorm[i-1]);
    }
    for (j=1; j<=nHalfFIR; j++) {          // Coefficients "100 to 198"
        q = MF_PI*(float32_t)j;
        // First, deal with the zero frequency end band that is "low-pass."
        cf32f[j+nHalfFIR] = aVolts[0]*sinf(fNorm[0]*2.0*q)/q;
        //  and then the rest of the bands that have low and high frequencies
        for(i=1; i<nBands; i++)
           cf32f[j+nHalfFIR] += aVolts[i]*( (sinf(fNorm[i]*2.0*q)/q) - (sinf(fNorm[i-1]*2.0*q)/q) );
    }

    /* At this point, the cf32f[] coefficients are simply truncated sin(x)/x shapes, creating
     * very high sidelobe responses. To reduce the sidelobes, a windowing function is applied.
     * This has the side affect of increasing the rate of cutoff for sharp frequency changes.
     * The only windowing function available here is that of James Kaiser.  This has a number
     * of desirable features.  The tradeoff of sidelobe level versus cutoff rate is variable. 
     * We specify it in terms of kdb, the highest sidelobe, in dB, next to a sharp cutoff. For
     * calculating the windowing vector, we need a parameter beta, found as follows:
     */
    if (kdb<0) return ERR_EQ_SIDELOBES;
    if (kdb>50)
        beta = 0.1102*(kdb-8.7);
    else if  (kdb>20.96 && kdb<=50.0)
        beta = 0.58417*powf((kdb-20.96), 0.4) + 0.07886*(kdb-20.96);
    else
        beta=0.0;
    // Note: i0f is the floating point in & out zero'th order Bessel function (see mathDSP_F32.h)
    kbes = 1.0f / mathEqualizer.i0f(beta);      // An additional derived parameter used in loop
  
    // Apply the Kaiser window
    scaleXj2 = 2.0f/(float32_t)nFIR;
    scaleXj2 *= scaleXj2;
    for (j=0; j<=nHalfFIR; j++) {  // For 199 Taps, this is 0 to 99
         xj2 = (int16_t)(0.5f+(float32_t)j);
         xj2 = scaleXj2*xj2*xj2;
         WindowWt=kbes*(mathEqualizer.i0f(beta*sqrt(1.0-xj2)));
         cf32f[nHalfFIR + j] *= WindowWt;          // Apply the Kaiser window to upper half
         cf32f[nHalfFIR - j] = cf32f[nHalfFIR +j]; // and create the lower half
    }
    // And fill in the members of fir_inst
    arm_fir_init_f32(&fir_inst, nFIR, (float32_t *)cf32f,  &StateF32[0], (uint32_t)block_size);
    return 0;
}

/* Calculate response in dB.  Leave nFreq point result in array rdb[] supplied
 * by the calling .INO  See Parks and Burris, "Digital Filter Design," p27 (Type 1).
 */
void AudioFilterEqualizer_F32::getResponse(uint16_t nFreq, float32_t *rdb)  {
	uint16_t i, j;
	float32_t bt;
	float32_t piOnNfreq;
	uint16_t nHalfFIR;

    nHalfFIR = (nFIR - 1)/2;
    piOnNfreq = MF_PI / (float32_t)nFreq;
    for (i=0; i<nFreq; i++) {
        bt = cf32f[nHalfFIR];//bt = 0.5f*cf32f[nHalfFIR];  // Center coefficient
        for (j=0; j<nHalfFIR; j++)  // Add in the others twice, as they are symmetric
            bt += 2.0f*cf32f[j]*cosf(piOnNfreq*(float32_t)((nHalfFIR-j)*i));
        rdb[i] = 20.0f*log10f(fabsf(bt));     // Convert to dB
    }
}
