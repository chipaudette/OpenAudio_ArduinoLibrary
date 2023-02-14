/*
 *   radioCESSB_Z_transmit_F32.cpp
 * This version of CESSB is intended for Zero-IF hardware.
 *
 * Bob Larkin, in support of the library:
 * Chip Audette, OpenAudio, Dec 2022
 *
 * MIT License,  Use at your own risk.
 *
 * See radioCESSB_Z_transmit_F32.h for technical info.
 *
 */
// NOTE:  96 ksps sample rate not yet implemented
#include "radioCESSB_Z_transmit_F32.h"

void radioCESSB_Z_transmit_F32::update(void)  {
   audio_block_f32_t *blockIn, *blockOutI, *blockOutQ;

   // Temporary storage.  At an audio sample rate of 96 ksps, the used
   // space will be half of the declared space.
   float32_t HilbertIn[32];
   float32_t workingDataI[128];
   float32_t workingDataQ[128];
   float32_t delayedDataI[64];  // Allows batching of 64 data points
   float32_t delayedDataQ[64];
   float32_t diffI[64];
   float32_t diffQ[64];

   if(sampleRate!=SAMPLE_RATE_44_50  &&  sampleRate!=SAMPLE_RATE_88_100)
      return;

   // Get all needed resources, or return if not available.
   blockIn = AudioStream_F32::receiveReadOnly_f32();
   if (!blockIn)
      { return; }
   blockOutI = AudioStream_F32::allocate_f32(); // a block for I output
   if (!blockOutI)
      {
      AudioStream_F32::release(blockIn);
      return;
      }
   blockOutQ = AudioStream_F32::allocate_f32();  // and for Q
   if (!blockOutQ)
      {
      AudioStream_F32::release(blockOutI);
      AudioStream_F32::release(blockIn);
      return;
      }
   // The audio input peak levels for start of CESSB are -1.0, 1.0
   // when gainIn==1.0.

/* // A +/- pulse to test timing of various delays
   // PULSE TEST  for diagnostics only
   for(int kk=0; kk<128; kk++)
      {
      uint16_t y=(ny & 1023);
      // pulse max at  is just starting to clip

      if     (y>=100 && y<115)  blockIn->data[kk] = 4.189f;
      else if(y>=115 && y<130)  blockIn->data[kk] = -4.189f;
      else blockIn->data[kk] = 0.0f;
      ny++;
      // Serial.println(blockIn->data[kk]);
      }
 */
   // uint32_t ttt=micros();

   // Decimate 48 ksps to 12 ksps, 128 to 32 samples
   //       or 96 ksps to 12 ksps, 128 to 16 samples
   arm_fir_decimate_f32(&decimateInst, &(blockIn->data[0]),
                        &HilbertIn[0], 128);
  // We now have nW=32 (for 48 ksps) or 16 (for 96 ksps) samples to process

  // Apply the Hilbert transform FIR.
  arm_fir_f32(&firInstHilbertI, &HilbertIn[0], &workingDataI[0], nW);

     /* ======= Sidebar:  Circular 2^n length delay arrays ========
      *
      * The length of the array, N,
      * must be a power of 2.  For example N=2^6 = 64.  The minimum
      * delay possible is the trivial case of 0 up to N-1.
      * As in C, let i be the index of the N array elements which
      * would range from 0 to N-1.  If p is an integer, that is a power
      * of 2 also, with p >= n, it can serve as an index to the
      * delay array by "ANDing" it with (N-1).  That is,
      * i = p & (N-1).  It can be convenient if the largest
      * possible value of the integer p, plus 1, is an integer multiple
      * of the arrray size N, as then the rollover of p will not cause
      * a jump in i.  For instance, if p is an uint8_t with a maximum
      * value of pmax=255, (pmax+1)/N = (255+1)/64 = 4, which is an
      * integer.  This combination will have no problems from rollover
      * of p.
      *
      * The new data point is entered at index p & (N - 1).  To
      * achieve a delay of d, the output of the delay array is taken
      * at index ((p-d) & (N-1)). The index is then incremented by 1.
      *
      * There are three delay lines of this construction below, starting
      * with delayHilbertQ
      *  ========================================================== */

    // Circular delay line for signal to align data with Hilbert FIR output
    // nW (32 for 48ksps) points into and out of the delay array
    for(uint16_t i=0;  i<nW;  i++)   // Let it wrap around at 128
        {
        // Put data point into the delay arrays, and do LSB/USB changing
        if(sidebandReverse)
           delayHilbertQ[indexDelayHilbertQ & 0X7F] = -HilbertIn[i];
        else
           delayHilbertQ[indexDelayHilbertQ & 0X7F] = HilbertIn[i];
        // Remove delayed data from line
        workingDataQ[i] = delayHilbertQ[(indexDelayHilbertQ - 100) & 0X7F];
        indexDelayHilbertQ++;
	    }

   // To compensate for splitting the signal into I & Q thereby doubling
   // the power, we add 0.707 factor.
   float32_t gainFactor = 0.70710678f*gainIn;
   for(int k=0; k<nW; k++)
      {
	  workingDataI[k] *= gainFactor;
	  workingDataQ[k] *= gainFactor;
      }

   // Mesaure input power and peak envelope, SSB before any CESSB processing
   for(int k=0; k<nW; k++)
      {
      float32_t pwrWorkingData = workingDataI[k]*workingDataI[k] + workingDataQ[k]*workingDataQ[k];
      float32_t vWD = sqrtf(pwrWorkingData);   // Envelope
      powerSum0 += pwrWorkingData;
      if(vWD > maxMag0)
         maxMag0 = vWD;              // Peak envelope
      countPower0++;
      }

   // Interpolate by 2 up to 24 ksps rate
   for(int k=0; k<nW; k++)   // 48 ksps:  0 to 31
      {
      int k2 = 2*(nW - k) - 1;  // 48 ksps 63 to 1
      // Zero pack, working from the bottom to not overwrite
      workingDataI[k2] = 0.0f;     // 64 element array
      workingDataI[k2-1]   = workingDataI[nW-k-1];
      workingDataQ[k2] = 0.0f;
      workingDataQ[k2-1]   = workingDataQ[nW-k-1];
      }

   // LPF with gain of 2 built into coefficients, correct for added zeros.
   arm_fir_f32(&firInstInterpolate1I,  workingDataI, workingDataI, nC);
   arm_fir_f32(&firInstInterpolate1Q,  workingDataQ, workingDataQ, nC);
   // WorkingDataI and Q are now at 24 ksps and ready for clipping
   // For input 48 ksps this produces 64 numbers

   for(int kk=0; kk<nC; kk++)
      {
      float32_t power = workingDataI[kk]*workingDataI[kk] + workingDataQ[kk]*workingDataQ[kk];
      float32_t mag = sqrtf(power);
      if(mag > 1.0f)  // This the clipping, scaled to 1.0, desired max
         {
         workingDataI[kk] /= mag;
         workingDataQ[kk] /= mag;
         }
      }

   // clipperIn needs spectrum control, so LP filter it.
   // Both BW of the signal and the sample rate have been doubled.
   arm_fir_f32(&firInstClipperI, workingDataI, workingDataI, nC);
   arm_fir_f32(&firInstClipperQ, workingDataQ, workingDataQ, nC);
   // Ready to compensate for filter overshoots
   for (int k=0; k<nC; k++)
      {
      // Circular delay line for signal to align data with FIR output
      // Put I & Q data points into the delay arrays
      osDelayI[indexOsDelay & 0X3F] = workingDataI[k];
      osDelayQ[indexOsDelay & 0X3F] = workingDataQ[k];
      // Remove 64 points delayed data from line and save for later
      delayedDataI[k] = osDelayI[(indexOsDelay - 63) & 0X3F];
      delayedDataQ[k] = osDelayQ[(indexOsDelay - 63) & 0X3F];
      indexOsDelay++;
      // Delay line to allow strongest envelope to be used for compensation
      // We only need to look ahead 1 or behind 1, so delay line of 4 is OK.
      // Enter latest envelope to delay array
      osEnv[indexOsEnv & 0X03] = sqrtf(
         workingDataI[k]*workingDataI[k] + workingDataQ[k]*workingDataQ[k]);

      // look over the envelope curve to find the max
      float32_t eMax = 0.0f;
      if(osEnv[(indexOsEnv) & 0X03] > eMax)    // Data point just entered
         eMax = osEnv[(indexOsEnv) & 0X03];
      if(osEnv[(indexOsEnv-1) & 0X03] > eMax)  // Entered one before
         eMax = osEnv[(indexOsEnv-1) & 0X03];
      if(osEnv[(indexOsEnv-2) & 0X03] > eMax)  // Entered one before that
         eMax = osEnv[(indexOsEnv-2) & 0X03];
      if(eMax < 1.0f)
         eMax = 1.0f;                          // Below clipping region
      indexOsEnv++;

      // Clip the signal to 1.0.  -2 allows 1 look ahead on signal.
      float32_t eCorrectedI = osDelayI[(indexOsDelay - 2) & 0X3F] / eMax;
      float32_t eCorrectedQ = osDelayQ[(indexOsDelay - 2) & 0X3F] / eMax;
      // Filtering is linear, so we only need to filter the difference between
      // the signal and the clipper output.  This needs less filtering, as the
      // difference is many dB below the signal to begin with. Hershberger 2014
      diffI[k] = osDelayI[(indexOsDelay - 2) & 0X3F] - eCorrectedI;
      diffQ[k] = osDelayQ[(indexOsDelay - 2) & 0X3F] - eCorrectedQ;
      }  // End, for k=0 to 63

   // Filter the differences, osFilter has 123 taps and 61 delay
   arm_fir_f32(&firInstOShootI, diffI, diffI, nC);
   arm_fir_f32(&firInstOShootQ, diffQ, diffQ, nC);

    // Do the overshoot compensation
   for(int k=0; k<nC; k++)
      {
      workingDataI[k] = delayedDataI[k] - gainCompensate*diffI[k];
      workingDataQ[k] = delayedDataQ[k] - gainCompensate*diffQ[k];
      }

   // Measure average output power and peak envelope, after CESSB
   // but before gainOut
   for(int k=0; k<nC; k++)
      {
      float32_t pwrOut = workingDataI[k]*workingDataI[k] +
           workingDataQ[k]*workingDataQ[k];
      float32_t vWD = sqrtf(pwrOut);   // Envelope
      powerSum1 += pwrOut;
      if(vWD > maxMag1)
         maxMag1 = vWD;              // Peak envelope
      countPower1++;
      }

   // Optional corrections to compensate for external hardware errors
   if(useIQCorrection)
      {
	  for(int k=0; k<nC; k++)
	     {
         workingDataI[k] *= gainI;
         workingDataI[k] += crossIQ*workingDataQ[k];
         workingDataQ[k] += crossQI*workingDataI[k];
	     }
      }

   // Finally interpolate to 48 or 96 ksps. Data is in workingDataI[k]
   // and is 64 samples for audio 48 ksps.
   for(int k=0; k<nC; k++)   // Audio sampling at 48 ksps:  0 to 63
      {
      int k2 = 2*(nC - k) - 1;  // 48 ksps 63 to 1
      // Zero pack, working from the bottom to not overwrite
      workingDataI[k2]   = 0.0f;
      workingDataI[k2-1] = gainOut*workingDataI[nC-k-1];  // gainOut does not change CESSB
      workingDataQ[k2]   = 0.0f;
      workingDataQ[k2-1] = gainOut*workingDataQ[nC-k-1];  // ...it just scales the level
      }
   // LPF with gain of 2 built into coefficients, correct for zeros.
   arm_fir_f32(&firInstInterpolate2I,  workingDataI, &blockOutI->data[0], 128);
   arm_fir_f32(&firInstInterpolate2Q,  workingDataQ, &blockOutQ->data[0], 128);
   // Voltage gain from blockIn->data to here for small sine wave is 1.0

    AudioStream_F32::transmit(blockOutI, 0); // send the outputs
    AudioStream_F32::transmit(blockOutQ, 1);
    AudioStream_F32::release(blockIn);       // Release the blocks
    AudioStream_F32::release(blockOutI);
    AudioStream_F32::release(blockOutQ);

    jjj++;   //For test printing
    // Serial.println(micros() - ttt);
}   // end update()
