/*
 * Process_DSP_R.ino 
 * Basically the Hill code with changes for Teensy floating point
 * OpenAudio_ArduinoLibrary.
 * Bob Larkin W7PUA, September 2022.
 *
 */
/* Thank you to Charley Hill, W5BAA, https://github.com/Rotron/Pocket-FT8
 * for the conversion to Teensy operation, as well as
 * to Kārlis Goba, YL3JG, https://github.com/kgoba/ft8_lib.
 * Thanks to all the contributors to the Joe Taylor WSJT project.
 * See "The FT4 and FT8 Communication Protocols," Steve Franks, K9AN,
 * Bill Somerville, G4WJS and Joe Taylor, K1JT, QEX July/August 2020
 * pp 7-17 as well as https://www.physics.princeton.edu/pulsar/K1JT
 */

/*  *****  MIT License  ***
Copyright (C) 2021, Charles Hill
Copyright (C) 2022, Bob Larkin  on changes for F32 library
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
/* NOTE - The frequency range used here is the same as used by others.
 * This is about bin 128 to 768, or 400 Hz to  2400 Hz.
 * Stations do operate outside this range.  It would be easy to
 * increase the range here.  The library function radioFT8Demodulator_F32 is filtered
 * to pass all frequencies up to, at least 2800 Hz.
 */

float32_t   window[1024];  // Holds half of symmetrical curve
arm_rfft_fast_instance_f32 Sfft;

void init_DSP(void) {
   arm_rfft_fast_init_f32(&Sfft, 2048);
   // The window is symmetric, so create half of it
   for (int i = 0; i < 1024; ++i)
     window[i] = ft_blackman_i(i, 2048);
   offset_step = 1472;    // (int) HIGH_FREQ_INDEX*4;
   }

float ft_blackman_i(int i, int N) {
    const float alpha = 0.16f; // or 2860/18608
    const float a0 = (1 - alpha) / 2;
    const float a1 = 1.0f / 2;
    const float a2 = alpha / 2;

    float x1 = cosf(2 * (float)M_PI * i / (N - 1));
    //float x2 = cosf(4 * (float)M_PI * i / (N - 1));
    float x2 = 2*x1*x1 - 1; // Use double angle formula
    return a0 - a1*x1 + a2*x2;
   }

// Compute FFT magnitudes (log power) for each timeslot in the signal
void extract_power( int offset) {
   float32_t fft_buffer[2048];
   float32_t fftOutput[2048];
   float32_t powerSum = 0.0f;    // Use these for snr estimate
   float32_t runningSum = 0.0f;
   float32_t powerMax = 0.0f;
   float32_t runningMax = 0.0f;
   float32_t noiseBuffer[8];     // Circular storage
   uint16_t  noiseBufferWrite = 0;   // Array index
   uint8_t   noisePower8 = 0;         // half dB per noise estimate GLOBAL
	
   float32_t y[8];
   float32_t noiseCoeff[3];

   /* The FFT's are arranged to have overlap in both time and frequency.
    * This allows good decoding sensitivity.  It remains difficult to decode a weak
	* signal that is close in frequency to a strong one.  It also causes multiple
	* combinations of time and frequency to show the same signal.
	* 
	* The 2048 point real FFT yields 1024 power outputs, corresponding to 0 to 3200 Hz.
	* Only the frequencies up to 2300 Hz are inspected in this implementation.  That
	* corresponds with outputs 0 to 736.  The windowing of the FFT results in frequency
	* resolution that is close to double the bin spacing.  So, the power data is grouped
	* up in 736/2=368 power data points.  It is done twice, using every other bins.
    * Format of export_fft_power[] array:
    * 368 bytes of power for even frequencies, 0, 2, 4, ... 366
    * 368 bytes of power for odd frequencies, 1, 3, 5, ... 367
    * Repeated 14.72/(0.08 sec) = 184 times.
	* The transmitted message length is 12.96 sec so the difference allows timing errors
	* along with the decoding allowing missing symbols.
    * Total bytes saved for decoding is 2 * 368 * 194 = 135424 for the 14.72 seconds.
    *
    * The power byte is log encoded with a half dB MSB.  This can handle a
    * dynamic range of 256/2 = 128 dB.
    */
	
	// TODO: It would seem easy to offset the frequencies being examined and, without
	// increasing the data burden, make the range more productive.  For instance, move the
	// 0 to 2300 up to 300 to 2600 Hz.  Needs investigation.
	
	// TODO: The 135424 byte array is using more than a fourth of Teensy 4.x main RAM1.
	// Might the array be put in RAM2 (and perhaps enlarged somewhat) and re-arranged
    // in frequency order.  This would allow transfers to RAM1 in, say, 300 Hz overlapping
	// segments.   This might be 300 to 600 Hz, 500 to 800, 700 to 1000 and so forth.  That
	// would cut way back on RAM 1 array size.  Problems with this, except complexity??

   // Note: The RadioFT8Demodulator provides at least 2.7 milliseconds, after data is
   // available, before pData2K is written over.  This needs to be thought of in the design
   // of the loop() in the main INO.  Long delays are trouble.  The following transfer
   // of data to fft_buffer is very fast.  After that, pData2K[] is available.
   for(int i=0; i<1024; i++)
      {
      fft_buffer[i] = window[i]*pData2K[i];  // Protect pData2K from in-place FFT (17 uSec)
	  fft_buffer[2047 - i] = window[i]*pData2K[2047 - i]; // Symmetrical window
      }
   //           (float32_t* pIn, float32_t* pOut, uint8_t ifftFlag)
   arm_rfft_fast_f32(&Sfft, fft_buffer, fftOutput, 0);
   arm_cmplx_mag_squared_f32(fftOutput, fftOutput, 1024);

   // Variables for estimating noise level for SNR
   powerSum = 0.0f;
   powerMax = 0.0f;

   for(int i=1; i<1024; i++)
      {
	  if(i>=128 && i<768)   // Omit the first 400 Hz and last 800 Hz
		 powerSum += fftOutput[i];
	  if(fftOutput[i] > powerMax)
		 powerMax = fftOutput[i];
      // Next, 20*log10() (not 10*) is to the make 8-bit resolution 0.5 dB.
      // The floats range from nothing to 40*log10(1024)=120 for a
      // pure sine wave. For FT8, we never encounter this.  To keep
      // the sine wave answer below 256 would use an upward
      // offset of 256-120=136.   This totally prevents overload!
      // Borrow fft_buffer for a moment:
      fft_buffer[i] = 136.0f + 20.0f*log10f( 0.0000001f + fftOutput[i] );
      }
   fft_buffer[0] = 0.000001;  // Fake DC term

   /* Noise needs to be estimated to determine snr. Two cases:
    * runningMax/runningSum < 100  This is weak signal case for which
	*                              the runningSum must be used alone.
	* runningMax/runningSum > 100  Here the 2 second quiet period can
	*                              can be found and running Sum used
	*                              when runningMax/runningSum is high.
	*/
   runningSum = 0.80f*runningSum + 0.20f*powerSum;    // Tracks changes in pwr
   runningMax = 0.99f*runningMax + 0.01f*powerMax;  // Slow decay
   // Put the sum intocircular buffer
   noiseBuffer[ 0X0007 & noiseBufferWrite++ ] = 0.00156f*runningSum;
   for(int kk=0; kk<8; kk++)
	   y[kk] = (float32_t)noiseBuffer[ 0X0007 & (kk + noiseBufferWrite) ];
   //fitCurve (int order, int nPoints, float32_t py[], int nCoeffs, float32_t *coeffs)
   fitCurve(2, 8, y, 3, noiseCoeff);
   float32_t y9 = noiseCoeff[2] + 9.0f*noiseCoeff[1] + 81.0f*noiseCoeff[0];

   if(runningMax > 100.0f*0.00156f*runningSum  &&  y9 > 2.0f*noiseCoeff[2]  && !noiseMeasured)
      {
      // This measurement occurs once every 15 sec, but may be just before
	  // or just after decode.  Either way, the "latest" noise estimate is used.
	  noiseMeasured = true;     // Reset after decode()
	  FT8noisePowerEstimateH = 0.2f*(y[0]+y[1]+y[2]+y[3]+y[4]);
	  FT8noisePwrDBIntH = (int16_t)(10.0f*log10f(FT8noisePowerEstimateH));
	  FT8noisePeakAveRatio = runningMax/(0.00156*runningSum);
#ifdef DEBUG_N
      Serial.println("Noise measurement between transmit time periods:");
	  Serial.print("   rSum, rMax= ");  Serial.print(0.00156*runningSum, 5);
      Serial.print("  ");  Serial.print(runningMax, 5);
      Serial.print(" Ratio= "); Serial.print(FT8noisePeakAveRatio, 3);
      Serial.print(" Int noise= ");
      Serial.println(FT8noisePwrDBIntH);   //  dB increments
#endif
      }

   // Loop over two frequency bin offsets.  This first picks up 367 even
   // numbered fft_buffer[] followed by 367 odd numbered bins.  This is
   // a frequency shift of 3.125 Hz.  With windowing, the bandwidth
   // of each FFT output is about 6 Hz, close to a match for the
   // 0.16 sec transmission time.
   /* First pass:  j on (0, 367)  j*2+freq_sub on (0, 734)   (even)
    * Secnd pass:  j on (0, 367)  j*2+freq_sub on (1, 735)   (odd)
    */
   for (int freq_sub=0; freq_sub<2; ++freq_sub)
      {
      for (int j=0; j<368; ++j)
         {
         export_fft_power[offset] = (uint8_t)fft_buffer[j*2 + freq_sub];
         ++offset;
         }
      }
   }           // End extract_power()

// ===============================================================
// CURVE FIT

/*
  curveFitting - Library for fitting curves to given
  points using Least Squares method, with Cramer's rule
  used to solve the linear equation.
  Created by Rowan Easter-Robinson, August 23, 2018.
  Released into the public domain.

  Converted to float32_t, made specific to FT8 case  Bob L  Oct 2022
 */

void cpyArray(float32_t *src, float32_t*dest, int n){
  for (int i = 0; i < n*n; i++){
    dest[i] = src[i];
  }
}

void subCol(float32_t *mat, float32_t* sub, uint8_t coln, uint8_t n){
  if (coln >= n) return;
  for (int i = 0; i < n; i++){
    mat[(i*n)+coln] = sub[i];
  }
}

/*Determinant algorithm taken from
// https://codeforwin.org/2015/08/c-program-to-find-determinant-of-matrix.html */
int trianglize(float32_t **m, int n)
{
  int sign = 1;
  for (int i = 0; i < n; i++) {
    int max = 0;
    for (int row = i; row < n; row++)
      if (fabs(m[row][i]) > fabs(m[max][i]))
        max = row;
    if (max) {
      sign = -sign;
      float32_t *tmp = m[i];
      m[i] = m[max], m[max] = tmp;
    }
    if (!m[i][i]) return 0;
    for (int row = i + 1; row < n; row++) {
      float32_t r = m[row][i] / m[i][i];
      if (!r) continue;
      for (int col = i; col < n; col ++)
        m[row][col] -= m[i][col] * r;
    }
  }
  return sign;
}

float32_t det(float32_t *in, int n)
{
  float32_t *m[n];
  m[0] = in;

  for (int i = 1; i < n; i++)
    m[i] = m[i - 1] + n;
  int sign = trianglize(m, n);
  if (!sign)
    return 0;
  float32_t p = 1;
  for (int i = 0; i < n; i++)
    p *= m[i][i];
  return p * sign;
}
/*End of Determinant algorithm*/

//Raise x to power
float32_t curveFitPower(float32_t base, int exponent){
  if (exponent == 0){
    return 1;
  } else {
    float32_t val = base;
    for (int i = 1; i < exponent; i++){
      val = val * base;
    }
    return val;
  }
}

#define MAX_ORDER 4
int fitCurve (int order, int nPoints, float32_t py[], int nCoeffs, float32_t *coeffs) {
  int i, j;
  float32_t T[MAX_ORDER] = {0}; //Values to generate RHS of linear equation
  float32_t S[MAX_ORDER*2+1] = {0}; //Values for LHS and RHS of linear equation
  float32_t denom; //denominator for Cramer's rule, determinant of LHS linear equation
  float32_t x, y;
  float32_t px[nPoints]; //Generate X values, from 0 to n

  for (i=0; i<nPoints; i++){
	px[i] = i;
  }

  for (i=0; i<nPoints; i++) {//Generate matrix elements
    x = px[i];
    y = py[i];
    for (j = 0; j < (nCoeffs*2)-1; j++){
      S[j] += curveFitPower(x, j); // x^j iterated , S10 S20 S30 etc, x^0, x^1...
    }
    for (j = 0; j < nCoeffs; j++){
      T[j] += y * curveFitPower(x, j); //y * x^j iterated, S01 S11 S21 etc, x^0*y, x^1*y, x^2*y...
    }
  }

  float32_t masterMat[nCoeffs*nCoeffs]; //Master matrix LHS of linear equation
  for (i = 0; i < nCoeffs ;i++){//index by matrix row each time
    for (j = 0; j < nCoeffs; j++){//index within each row
      masterMat[i*nCoeffs+j] = S[i+j];
    }
  }

  float32_t mat[nCoeffs*nCoeffs]; //Temp matrix as det() method alters the matrix given
  cpyArray(masterMat, mat, nCoeffs);
  denom = det(mat, nCoeffs);
  cpyArray(masterMat, mat, nCoeffs);

  //Generate cramers rule mats
  for (i = 0; i < nCoeffs; i++){ //Temporary matrix to substitute RHS of linear equation as per Cramer's rule
    subCol(mat, T, i, nCoeffs);
    coeffs[nCoeffs-i-1] = det(mat, nCoeffs)/denom; //Coefficients are det(M_i)/det(Master)
    cpyArray(masterMat, mat, nCoeffs);
  }
  return 0;
}
