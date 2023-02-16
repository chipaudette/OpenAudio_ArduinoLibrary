/*
 *   radioCESSBtransmit_F32.cpp
 *
 * Bob Larkin, Dec 2022, in support of the library:
 * Chip Audette, OpenAudio_ArduinoLibrary
 *
 * MIT License,  Use at your own risk.
 *
 * See radioCESSBtransmit_F32.h for technical info.
 *
 */

#include "radioCESSBtransmit_F32.h"
// 513 values of the sine wave in a float array:
#include "sinTable512_f32.h"

// sincos(ph) inputs phase on (0, 512) and outputs private sn, cs
// A simplified version of the F32 synthesizer class
// AudioSynthSineCosine_F32.  Full F32 accuracy
void radioCESSBtransmit_F32::sincos(float32_t ph)  {
   uint16_t index;
   float32_t a, b, deltaPhase;

   index = (uint16_t)ph;
   deltaPhase = ph -(float32_t)index;
   /* Read two nearest values of input value from the sin table */
   a = sinTable512_f32[index];
   b = sinTable512_f32[index+1];
   sn = a+(b-a)*deltaPhase;  /* Linear interpolation process */
   /* Repeat for cosine by adding 90 degrees phase  */
   index = (index + 128) & 0x01ff;
   /* Read two nearest values of input value from the sin table */
   a = sinTable512_f32[index];
   b = sinTable512_f32[index+1];
   /* deltaPhase will be the same as used for sin  */
   cs = a +(b-a)*deltaPhase;  /* Linear interpolation process */
//   if(ttt++ <100){Serial.print(ttt); Serial.print(","); Serial.println(sn, 8); } <<<<<<
   }

void radioCESSBtransmit_F32::update(void)  {
   audio_block_f32_t *blockIn, *blockOutI, *blockOutQ;

   // Temporary storage.  At an audio sample rate of 96 ksps, the used
   // space will be half of the declared space.
   // Todo: Cut 1 or two arrays out by more sharing
   float32_t weaverIn[32];
   float32_t weaverMI[32];
   float32_t weaverMQ[32];
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

/* A +/- pulse to test timing of various delays.  PULSE TEST
 * This replaces any input from the audio stream,
 * and levels shown are for gainIn==1.0.
   for(int kk=0; kk<128; kk++)
      {
      uint16_t y=(ny & 1023);
      // pulse max at 1.548 is just starting to clip
      // 2.189 is 3 dB increase
      if     (y>=100 && y<115)  blockIn->data[kk] = 2.189f;
      else if(y>=115 && y<130)  blockIn->data[kk] = -2.189f;
      else blockIn->data[kk] = 0.0f;
      ny++;
      // Serial.println(blockIn->data[kk]);
      }   */

   // Decimate 48 ksps to 12 ksps, 128 to 32 samples
   //       or 96 ksps to 12 ksps, 128 to 16 samples (not yet)
   arm_fir_decimate_f32(&decimateInst, &(blockIn->data[0]),
                        &weaverIn[0], 128);

   // We now have 32 or 16 samples to process and interpolate out
   float32_t gainIn2 = 2.0f*gainIn;  // 2 because the mixers are 0.5
   for(int k=0; k<nW; k++)
      {
      weaverIn[k] *= gainIn2;     // Input gain for CESSB

      phaseW += phaseIncrementW;
      if(phaseW >=512.0f)
         phaseW -= 512.0f;
      sincos(phaseW);                 // Generate cs, sn
      if(sidebandReverse)
         weaverMI[k] = -weaverIn[k]*cs;   // Quadrature mixers
      else
         weaverMI[k] = weaverIn[k]*cs;
      weaverMQ[k] = weaverIn[k]*sn;
      }

   // Filter Weaver I and Q using first half of Out array.
   // Bandwidth at this point is 0 to 1350 Hz.
   arm_fir_f32(&firInstWeaverI, weaverMI, workingDataI, nW);
   arm_fir_f32(&firInstWeaverQ, weaverMQ, workingDataQ, nW);
   // Note: Sine wave envelope gain from blockIn->data[kk] to here is gainIn

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

   // LPF with gain of 2 built into coefficients, correct for zeros.
   arm_fir_f32(&firInstInterpolate1I,  workingDataI, workingDataI, nC);
   arm_fir_f32(&firInstInterpolate1Q,  workingDataQ, workingDataQ, nC);
   // WorkingDataI and Q are now at 24 ksps and ready for clipping
   // For input 48 ksps this produces 64 numbers
   // Voltage gain from blockIn->data to here for small sine wave is 1.0

   for(int kk=0; kk<nC; kk++)
      {
      float32_t power = workingDataI[kk]*workingDataI[kk] + workingDataQ[kk]*workingDataQ[kk];
      float32_t mag = sqrtf(power);
      if(mag > 1.0f)  // This the clipping, scaled to 1.0, desired max
         {
         workingDataI[kk] /= mag;
         workingDataQ[kk] /= mag;
         }
      powerSum0 += power;  // For measuring amount of clipping
      if(mag > maxMag0)
         maxMag0 = mag;
      }

   // clipperIn needs spectrum control, so LP filter it.  Same filter coeffs as Weaver.
   // Both BW of the signal and the sample rate have been doubled.
   arm_fir_f32(&firInstClipperI, workingDataI, workingDataI, nC);
   arm_fir_f32(&firInstClipperQ, workingDataQ, workingDataQ, nC);

   // Ready to compensate for filter overshoots
   for (int k=0; k<64; k++)
      {
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
      *  ========================================================== */

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
      if(osEnv[(indexOsEnv) & 0X03] > eMax)  // One just entered
         eMax = osEnv[(indexOsEnv) & 0X03];
      if(osEnv[(indexOsEnv-1) & 0X03] > eMax)  // Entered one before
         eMax = osEnv[(indexOsEnv-1) & 0X03];
      if(osEnv[(indexOsEnv-2) & 0X03] > eMax)  // Entered one before that
         eMax = osEnv[(indexOsEnv-2) & 0X03];
      if(eMax < 1.0f)
         eMax = 1.0f;                         // Below clipping region

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

   // Filter the differences, osFilter has 129 taps and 64 delay
   arm_fir_f32(&firInstOShootI, diffI, diffI, nC);
   arm_fir_f32(&firInstOShootQ, diffQ, diffQ, nC);

   // Do the overshoot compensation
   for(int k=0; k<64; k++)
      {
      workingDataI[k] = delayedDataI[k] - gainCompensate*diffI[k];
      workingDataQ[k] = delayedDataQ[k] - gainCompensate*diffQ[k];
      }

   // Finally interpolate to 48 or 96 ksps. Data is in workingDataI[k]
   // and is 64 samples for audio 48 ksps.
   for(int k=0; k<nC; k++)   // Audio sampling at 48 ksps:  0 to 63
      {
      int k2 = 2*(nC - k) - 1;  // 48 ksps 63 to 1
      // Zero pack, working from the bottom to not overwrite
      workingDataI[k2]   = 0.0f;
      workingDataI[k2-1] = workingDataI[nC-k-1];
      workingDataQ[k2]   = 0.0f;
      workingDataQ[k2-1] = workingDataQ[nC-k-1];
      }
   // LPF with gain of 2 built into coefficients, correct for zeros.
   arm_fir_f32(&firInstInterpolate2I,  workingDataI, &blockOutI->data[0], 2*nC);
   arm_fir_f32(&firInstInterpolate2Q,  workingDataQ, &blockOutQ->data[0], 2*nC);
   // Voltage gain from blockIn->data to here for small sine wave is 1.0

   // Measure output power and peak envelope, after CESSB
   for(int k=0; k<128; k++)
      {
      float32_t pwrOut = blockOutI->data[k]*blockOutI->data[k] + blockOutQ->data[k]*blockOutQ->data[k];
      float32_t vWD = sqrtf(pwrOut);   // Envelope
      powerSum1 += pwrOut;
      if(vWD > maxMag1)
         maxMag1 = vWD;              // Peak envelope
      countPower1++;
      }

    AudioStream_F32::transmit(blockOutI, 0); // send the outputs
    AudioStream_F32::transmit(blockOutQ, 1);
    AudioStream_F32::release(blockIn);       // Release the blocks
    AudioStream_F32::release(blockOutI);
    AudioStream_F32::release(blockOutQ);
}  // end update()
