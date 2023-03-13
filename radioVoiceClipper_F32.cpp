/*
 *   radioVoiceClipper_F32.cpp
 *
 * Bob Larkin, in support of the library:
 * Chip Audette, OpenAudio, Dec 2022
 * MIT License,  Use at your own risk.
 *
 * See radioVoiceClipper_F32.h for technical info.
 */
// NOTE:  96 ksps sample rate not yet implemented

#include "radioVoiceClipper_F32.h"

void radioVoiceClipper_F32::update(void)  {
   audio_block_f32_t *blockIn, *blockOut                 ;

   // Temporary storage.  Max size for 12 ksps where 128 points at input
   // and 256 at interpolated 24ksps
   float32_t workingData[256];
   float32_t delayedDataI[256];  // Allows batching of 64 data points
   float32_t diffI[256];

   if(sampleRate!=VC_SAMPLE_RATE_11_12 && sampleRate!=VC_SAMPLE_RATE_44_50  &&  sampleRate!=VC_SAMPLE_RATE_88_100)
       return;
   // Get all needed resources, or return if not available.
   blockIn = AudioStream_F32::receiveReadOnly_f32();
   if (!blockIn)
      { return; }
   blockOut = AudioStream_F32::allocate_f32();
   if (!blockOut)
      {
      AudioStream_F32::release(blockIn);
      return;
      }

   // The audio input peak levels for start of clipping are -1.0, 1.0
   // when gainIn==1.0.
   // uint32_t ttt=micros();

   if(sampleRate==VC_SAMPLE_RATE_11_12)
      {
      // No decimation, 128 samples 
      for(int k=0; k<128; k++)
         workingData[k] = blockIn->data[k];
     // We now have nW=128 (for 12 ksps) samples to process
      }
   else if(sampleRate==VC_SAMPLE_RATE_44_50)
      {
      // Decimate 48 ksps to 12 ksps, 128 to 32 samples
      //       or 96 ksps to 12 ksps, 128 to 16 samples
      arm_fir_decimate_f32(&decimateInst, &(blockIn->data[0]),
                        &workingData[0], 128);
      // We now have nW=32 (for 48 ksps) or 16 (for 96 ksps) samples to process
      }
   // Measure input power and peak envelope, before any clipping.
   for(int k=0; k<nW; k++)
      {
      float32_t pwrWorkingData = workingData[k]*workingData[k];  // Replace with absf() <<<<<<<<<<<<<<<<<<<<<<<<
      float32_t vWD = sqrtf(pwrWorkingData);   // Envelope
      powerSum0 += pwrWorkingData;
      if(vWD > maxMag0)
         maxMag0 = vWD;              // Peak envelope
      countPower0++;
      }

   for(int k=0; k<nW; k++)
      {
	  workingData[k] *= gainIn;   // Sets the amount of clipping for 1.0 in
//Serial.println(workingData[k]);
      }

   // Interpolate by 2 up to 24 ksps rate
   for(int k=0; k<nW; k++)      // 48 ksps:  0 to 31
      {
      int k2 = 2*(nW - k) - 1;  // 48 ksps: 63 to 1
      // Zero pack, working from the bottom to not overwrite
      workingData[k2] = 0.0f;   // 48 ksps: 64 element array
      workingData[k2-1]   = workingData[nW-k-1];
      }

   // LPF with gain of 2 built into coefficients, correct for added zeros.
   arm_fir_f32(&firInstInterpolate1I,  workingData, workingData, nC);
   // workingData are now at 24 ksps and ready for clipping
   // For input 48 ksps this produces 64 numbers

   for(int kk=0; kk<nC; kk++)
      {
      float32_t power = workingData[kk]*workingData[kk];  // Change to absf()
      float32_t mag = sqrtf(power);
      if(mag > 1.0f)  // This the clipping, scaled to 1.0, desired max
         {
         workingData[kk] /= mag;
         }
      }

   // clipperIn needs spectrum control, so LP filter it.
   // Both BW of the signal and the sample rate have been doubled.
   arm_fir_f32(&firInstClipperI, workingData, workingData, nC);

   // Ready to compensate for filter overshoots
   for (int k=0; k<nC; k++)
      {
      // Circular delay line for signal to align data with FIR output
      // Put I & Q data points into the delay arrays
      osDelayI[indexOsDelay & 0X3F] = workingData[k];
      // Remove 64 points delayed data from line and save for later
      delayedDataI[k] = osDelayI[(indexOsDelay - 63) & 0X3F];
      indexOsDelay++;
      // Delay line to allow strongest envelope to be used for compensation
      // We only need to look ahead 1 or behind 1, so delay line of 4 is OK.
      // Enter latest envelope to delay array
      osEnv[indexOsEnv & 0X03] = sqrtf(
         workingData[k]*workingData[k]);  // + workingDataQ[k]*workingDataQ[k]);

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
      // Filtering is linear, so we only need to filter the difference between
      // the signal and the clipper output.  This needs less filtering, as the
      // difference is many dB below the signal to begin with. Hershberger 2014
      diffI[k] = osDelayI[(indexOsDelay - 2) & 0X3F] - eCorrectedI;
      }  // End, for k=0 to 63

   // Filter the differences, osFilter has 123 taps and 61 delay
   arm_fir_f32(&firInstOShootI, diffI, diffI, nC);

    // Do the overshoot compensation
   for(int k=0; k<nC; k++)
      {
      workingData[k] = delayedDataI[k] - gainCompensate*diffI[k];
      }

   // Measure average output power and peak envelope, after CESSB
   // but before gainOut
   for(int k=0; k<nC; k++)
      {
      float32_t pwrOut = workingData[k]*workingData[k];
      float32_t vWD = sqrtf(pwrOut);   // Envelope
      powerSum1 += pwrOut;
      if(vWD > maxMag1)
         maxMag1 = vWD;              // Peak envelope
      countPower1++;
      }


   if(sampleRate==VC_SAMPLE_RATE_11_12)
      {
      // Decimat24 to 12, 128 samples out.  No LPF needed as we just did that
      for(int k=0; k<128; k++)
          blockOut->data[k] = workingData[2*k];
      }
   else if(sampleRate==VC_SAMPLE_RATE_44_50)
      {
      // Finally interpolate to 48 or 96 ksps. Data is in workingData[k]
      // and is 64 samples for audio 48 ksps.
      for(int k=0; k<nC; k++)   // Audio sampling at 48 ksps:  0 to 63
         {
         int k2 = 2*(nC - k) - 1;  // 48 ksps 63 to 1
         // Zero pack, working from the bottom to not overwrite
         workingData[k2]   = 0.0f;
         workingData[k2-1] = gainOut*workingData[nC-k-1];  // gainOut does not change CESSB
         }
      // LPF with gain of 2 built into coefficients, correct for zeros.
      arm_fir_f32(&firInstInterpolate2I,  workingData, &blockOut->data[0], 128);
      // Voltage gain from blockIn->data to here for small sine wave is 1.0
      }
   AudioStream_F32::transmit(blockOut, 0); // send the outputs
   AudioStream_F32::release(blockIn);       // Release the blocks
   AudioStream_F32::release(blockOut);

   jjj++;   //For test printing
   // Serial.println(micros() - ttt);
}  // end update()
