/* AudioEffectCompressor2_F32.cpp
 *
 * Bob Larkin  W7PUA  22 January 2021
 * See AudioEffectCompressor2_F32.h for details
 *
 * MIT License.  Use at your own risk.
 */
#include "AudioEffectCompressor2_F32.h"

 /* See https://github.com/Tympan/Tympan_Library/blob/master/src/AudioCalcGainWDRC_F32.h
  * Dr Paul Beckmann
  * https://community.arm.com/tools/f/discussions/4292/cmsis-dsp-new-functionality-proposal/22621#22621
  * Fast approximation to the log2() function.  It uses a two step
  * process.  First, it decomposes the floating-point number into
  * a fractional component F and an exponent E.  The fraction component
  * is used in a polynomial approximation and then the exponent added
  * to the result.  A 3rd order polynomial is used and the result
  * when computing db20() is accurate to 7.984884e-003 dB.  Y is log2(X)
  */
  float v2DB_Approx(float volts) {
     float Y, F;
     int E;
     // This is the approximation to log2()
     F = frexpf(volts, &E);    //   first separate power of 2;
     //  Y = C[0]*F*F*F + C[1]*F*F + C[2]*F + C[3] + E;
     Y = 1.23149591;   //C[0]
     Y *= F;
     Y += -4.11852516f; //C[1]
     Y *= F;
     Y += 6.02197014f;  //C[2]
     Y *= F;
     Y += -3.13396450f; //C[3]
     Y += E;
     // Convert to dB = 20 Log10(volts)
     return 6.020599f * Y;   // (20.0f/log2(10.0))*Y;
  }

 // Accelerate the powf(10.0,x) function (from Chip's single slope compressor)
 float pow10f(float x) {
    //return powf(10.0f,x)       //standard, but slower
    return expf(2.30258509f*x);  //faster:  exp(log(10.0f)*x)
 }

//  begin() 
void AudioEffectCompressor2_F32::begin(void)  {
    for(int kk =0; kk<5; kk++)   // Keeps division out of update()
       slope[kk] = 1.0f/curve0.compressionRatio[kk];

    outKneeDB[0] = curve0.marginDB;     // Start at top
    for(int kk=1; kk<5; kk++)  {
       outKneeDB[kk] = outKneeDB[kk-1] - (curve0.kneeDB[kk-1] -
            curve0.kneeDB[kk])/curve0.compressionRatio[kk-1];
       }

    firstIndex = 4;       // Start at bottom
    for(int kk=4; kk>0; kk--) {
       if(curve0.kneeDB[kk] >= -500.0)  {
          firstIndex = kk;
          break;
          }
       }
       /* for(int kk=0; kk<5; kk++) {
            Serial.print(kk); 
            Serial.print(" <--k outKneeDB[k]--> ");
            Serial.println(outKneeDB[kk]);
            }
       Serial.print("firstIndex--> ");
       Serial.println(firstIndex);
       */
    }

 //    update() 
 void AudioEffectCompressor2_F32::update(void) {
   float  vAbs, vPeak;
   float vInDB = 0.0f;
   float vOutDB = 0.0f;
   float targetGain;

   // Receive the input audio data
   audio_block_f32_t *block = AudioStream_F32::receiveWritable_f32();
   if (!block) return;
   // Allocate memory for the output
   audio_block_f32_t *out_block = AudioStream_F32::allocate_f32();
   if (!out_block)  {
      release(block);
      return;
      }

   // Find the smoothed envelope, target gain and compressed output
   vPeak = vPeakSave;
   for (int k=0; k<block->length; k++) {
       vAbs = (block->data[k] >= 0.0f) ? block->data[k] : -block->data[k];
       if (vAbs >= vPeak) {     // Attack (rising level)
           vPeak = alpha * vPeak + (oneMinusAlpha) * vAbs;
       } else {                 // Release (decay for falling level)
           vPeak = beta * vPeak;
           }
       // Convert to dB
       // At all levels and quite frequency flat, this under estimates by about 1.05 dB
       vInDB = v2DB_Approx(vPeak) + 1.05f;
       if(vInDB > vInMaxDB)  vInMaxDB = vInDB;  // For reporting back

       // Find gain point.  Don't look below first segment firstIndex.
       for(int kk=firstIndex; kk>=0; kk--)  {
          if( vInDB<=curve0.kneeDB[kk] || kk==0 )  {
              vOutDB = outKneeDB[kk] + slope[kk]*(vInDB - curve0.kneeDB[kk]);
              break;
              }
          }
       // Convert the needed gain back to a voltage ratio 10^(db/20)
       targetGain = pow10f(0.05f*(vOutDB - vInDB));
       // And apply target gain to signal stream from the delayed data. The
       // delay buffer is circular because of delayBufferMask and length 2^m m<=8.
       out_block->data[k] = targetGain * delayData[(k + in_index) & delayBufferMask];

       if(printIO) {
          Serial.print(block->data[k],6);
          Serial.print("," );
          Serial.print(delayData[(k + in_index) & delayBufferMask],6);
          Serial.print("," );
          Serial.println(targetGain);
          }

       // Put the new data into the delay line, delaySize positions ahead.
       // If delaySize==256, this will be the same location as we just got data from.
       delayData[(k + in_index + delaySize) & delayBufferMask] = block->data[k];
     }
   vPeakSave = vPeak;            // save last vPeak for next time
   sampleInputDB = vInDB;        // Last values for get...() functions
   sampleGainDB = vOutDB - vInDB;
   // transmit the block and release memory
   AudioStream_F32::release(block);
   AudioStream_F32::transmit(out_block); // send the FIR output
   AudioStream_F32::release(out_block);
   // Update pointer in_index to delay line for next 128 update
   in_index = (in_index + block->length) & delayBufferMask;
   }     // End update()

 // Sets a new compression curve by transferring structure
 void AudioEffectCompressor2_F32::setCompressionCurve(struct compressionCurve *_cCurve) {
    curve0.marginDB = _cCurve->marginDB;
    curve0.offsetDB = _cCurve->offsetDB;
    for(int kk=0; kk<5; kk++)  {
       // Also, adjust the input levels for offsetDB value
       curve0.kneeDB[kk] = _cCurve->kneeDB[kk] - curve0.offsetDB;
       curve0.compressionRatio[kk] = _cCurve->compressionRatio[kk];
       }
    }
