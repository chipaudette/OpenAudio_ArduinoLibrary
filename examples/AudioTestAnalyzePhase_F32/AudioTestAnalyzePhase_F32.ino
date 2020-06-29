/* Test AudioAnalyzePhase_F32.cpp   RSL 7 April 2020
 *  Generates 2 sine waves of different phase, but same frequency
 *  and measures the phase difference.
 */

//Include files
#include <AudioAnalyzePhase_F32.h>
//#include <AudioStream_F32.h>
//#include <AudioStream.h>
//#include <Arduino.h>
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>
//#include <math.h>

//Create audio objects
// Input object creates stream, even though not used. I16 object to allow T4.x.
AudioInputI2S              audioInI2S1;
AudioConvert_I16toF32      cnvt1;
// And the objects for the phase measurement:
AudioSynthWaveformSine_F32 sine1;
AudioSynthWaveformSine_F32 sine2;
AudioAnalyzePhase_F32      phase1;
AudioRecordQueue_F32       queue1_F; 
AudioConnection            patchCord1(audioInI2S1, 0, cnvt1,    0);
AudioConnection_F32        patchCord3(sine1,       0, phase1,   0);
AudioConnection_F32        patchCord4(sine2,       0, phase1,   1);
AudioConnection_F32        patchCord5(phase1,      0, queue1_F, 0);
AudioControlSGTL5000       sgtl5000_1; 

#define NBLOCKS 8
  float dt1[128*NBLOCKS];
  float *pq, *pd;
  int k, i;
  
// ====================  SETUP()  ============================================
void setup(void) {
  //Start the USB serial link (to enable debugging)
  Serial.begin(300); delay(500);

  AudioMemory(2);      // Allocate Int16 audio data blocks
  AudioMemory_F32(15);  // Allocate Float32 audio data blocks
  sgtl5000_1.enable();
  
  AudioNoInterrupts(); 
  sine1.amplitude(1.0);
  sine1.frequency(12345);
  sine1.phase(0.0);
  
  sine2.amplitude(1.0);
  sine2.frequency(12345);
  // The next call sets the phase difference
  sine2.phase(60.0);    // This phase reationship will measure +60 degrees

  AudioInterrupts();

  // The next argument can be set to 1 to show update() errors.  But, it will show false
  // errors before the audiostream is up and running.
  phase1.showError(0);  // For diagnostics
  
/* For variable pdConfig (default 0b1100):
 * Bit 0: 0=No Limiter (default)   1=Use limiter
 * Bit 2 and 1: 00=Use no acos linearizer    01=undefined
 *              10=Fast, math-continuous acos() (default)    11=Accurate acos()
 * Bit 3: 0=No scale of multiplier  1=scale to min-max (default)
 * Bit 4: 0=Output in degrees    1=Output in radians (default)
 * 
 * Uncomment one of the next 4 examples, or leave all 4 commented
 * out and get the default settings for begin(), using about
 * 123 microseconds for 128 block size.
 * Times tu are time spent in update() on T3.6 for full 128 point block.
 */
  //#1 - This uses min-max scaling, fast acos(), and FIR filtering
  phase1.setAnalyzePhaseConfig(FIR_LP_FILTER, NULL, 53, 0b01100);    // tu = 213 microseconds

  //#2 - This uses min-max scaling and the Accurate acos()
//  phase1.setAnalyzePhaseConfig(FIR_LP_FILTER, NULL, 53, 0b01110);    // tu <= 531 microseconds

  //#3 - This uses min-max scaling, IIR Filter and the no acos() linearization
//  phase1.setAnalyzePhaseConfig(IIR_LP_FILTER, NULL, 53, 0b01000);     // tu = 96  microseconds

  //#4 - This uses no scaling (use two magnitude 1.0 sine waves),
  //     and the no acos() linearization. No LP filtering
//  phase1.setAnalyzePhaseConfig(NO_LP_FILTER, NULL, 20, 0b10000);      // tu = 28 uSec

  i = 0;  k=0;
  queue1_F.begin();
  Serial.println("Setup complete.");
};

void loop(void) {    
  // Collect 128xNBLOCKS samples and output to Serial
  // This "if" will be active for i on (0, NBLOCKS-1)
  if (queue1_F.available() >= 1  && i>=0 && i<NBLOCKS) // See if 128 words are available
    {
    pq = queue1_F.readBuffer();
    pd = &dt1[128*i++];
    for(k=0; k<128; k++)
      {  if(i==2 && k==5) Serial.println(dt1[259]);
      *pd++ = *pq++;        // Save 128 words in dt1[]
      }
    queue1_F.freeBuffer();

  // If 128*NBLOCKS words are saved, send them out Serial
  if(i == NBLOCKS)
    {
    i = NBLOCKS + 1;   // Should stop all this
    queue1_F.end();  // No more data to queue1
    Serial.println("128 x NBLOCKS Data Points:");
    for (k=0; k<128*NBLOCKS; k++)
       Serial.println  (dt1[k],6);
    Serial.println();
    }
  }
}
