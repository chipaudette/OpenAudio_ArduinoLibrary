/*
 *  ToneDetect3.ino  Test the CTCSS tone detection
 *  using the OpenAudio_ArduinoLibrary analyze_CTCSS_F32 class.
 *  This is also an example of generating and detecting radio
 *  narrow-band frequency modulation (NBFM.  This covers the case where the
 *  FM deviation is in the same order as the modulation frequencies.
 *  CTCSS sub-audible tones (see Wikipedia) are widely used to
 *  allow stations to only hear the desired transmitters.  The frequencies
 *  are all lower than that of voice to allow separation by filtering.
 *
 * Bob Larkin 26 March 2021
 * Revised 22 Jan 2022.
 * Public Domain
 */
#include "AudioStream_F32.h"
#include "Arduino.h"
#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"

// #define OUTPUT_QUEUE

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will handle.
// The CTCSS detector supports a restricted st of sample rates (details below).

const float sample_rate_Hz = 44117.0f;
const int   audio_block_samples = 128;   // Use this - only one supported in CTCSS detector
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples); // Not used, all at default

const float CTCSSFreq = 103.500f;
/* FIR filter designed with http://t-filter.appspot.com
 * This seems to be a good I-F filter for 5 kHz deviation NBFM.
 * Sampling frequency: 44100 Hz
 * 0 Hz - 6300 Hz, att >-62.1 dB
 * 8000 Hz - 20000 Hz  Ripple = 0.06 dB
 * 21700 Hz - 22050 Hz att >-62.1 dB
 */
float firFM_BPF[82] = {   // Limit noise going to FM detector
  0.0000000000000000000,
  0.00008365261860118879,
  0.0006583187950336784,
 -0.0019407568703118934,
 -0.00009987233532798062,
  0.00032719088373367114,
  0.0015310693898116902,
  0.000955022003263755,
 -0.0014168349573391522,
 -0.0012792163039246237,
 -0.0028041109113356045,
  0.003423773414325396,
  0.0005879630239341677,
  0.005484189218478419,
 -0.005929117878887913,
 -0.00011592770526681765,
 -0.008306219532671027,
  0.007927691595232189,
  0.0011012110020438936,
  0.010098403965529463,
 -0.008003622491352998,
 -0.0051152410852304386,
 -0.00960701825591872,
  0.004774918283483265,
  0.013581334230222949,
  0.006115374322484461,
  0.0025168498118005173,
 -0.02728352305492423,
 -0.00013301923349161136,
 -0.013504140226622814,
  0.04621408618779683,
 -0.005938008431812001,
  0.026530407240648667,
 -0.0703999968346119,
  0.006639153066796368,
 -0.038963325061155324,
  0.10421524449162319,
  0.01319721488745073,
  0.04792495681421007,
 -0.19558525086465353,
 -0.21559102765412194,
  0.6154759104918608,
 -0.21559102765412194,
 -0.19558525086465353,
  0.04792495681421007,
  0.01319721488745073,
  0.10421524449162319,
 -0.038963325061155324,
  0.006639153066796368,
 -0.0703999968346119,
  0.026530407240648667,
 -0.005938008431812001,
  0.04621408618779683,
 -0.013504140226622814,
 -0.00013301923349161136,
 -0.02728352305492423,
  0.0025168498118005173,
  0.006115374322484461,
  0.013581334230222949,
  0.004774918283483265,
 -0.00960701825591872,
 -0.0051152410852304386,
 -0.008003622491352998,
  0.010098403965529463,
  0.0011012110020438936,
  0.007927691595232189,
 -0.008306219532671027,
 -0.00011592770526681765,
 -0.005929117878887913,
  0.005484189218478419,
  0.0005879630239341677,
  0.003423773414325396,
 -0.0028041109113356045,
 -0.0012792163039246237,
 -0.0014168349573391522,
  0.000955022003263755,
  0.0015310693898116902,
  0.00032719088373367114,
 -0.00009987233532798062,
 -0.0019407568703118934,
  0.0006583187950336784,
  0.00008365261860118879};

// Transmitter:
// Use SineCosine_F32 as it allows amplitudes greater than 
// 1.0 (this is FP and that is OK). Use sine channnel only.

AudioSynthSineCosine_F32 sine1;          //xy=62,181
AudioSynthGaussian_F32   noiseWhite1;    //xy=68.5,265
AudioSynthGaussian_F32   noiseWhite2;    //xy=68.5,379
AudioAnalyzeRMS_F32      rms2;           //xy=103,314
AudioFilterBiquad_F32    biQuad1;        //xy=223,265
AudioMixer4_F32          mixer4_1;       //xy=229,195
AudioMixer4_F32          mixer4_2;       //xy=236,381
RadioFMDetector_F32      FMDetector1;    //xy=258,476
AudioFilterFIR_F32       fir1;           //xy=365,381
radioModulatedGenerator_F32 modulator1;     //xy=395,189
AudioAnalyzeRMS_F32      rms1;           //xy=426,125
analyze_CTCSS_F32        toneDet1;       //xy=200,400
// AudioRecordQueue_F32     recordQueue1;   //xy=446,446
AudioOutputI2S_F32       audioOutI2S1;   //xy=448,489

AudioConnection_F32          patchCord1(sine1, 0, mixer4_1, 0);
AudioConnection_F32          patchCord2(noiseWhite1, biQuad1);
AudioConnection_F32          patchCord3(noiseWhite2, 0, mixer4_2, 1);
AudioConnection_F32          patchCord4(fir1, rms2);   // patchCord4(noiseWhite2, rms2);
AudioConnection_F32          patchCord5(biQuad1, 0, mixer4_1, 1);
AudioConnection_F32          patchCord6(mixer4_1, 0, modulator1, 1);
AudioConnection_F32          patchCord7(mixer4_2, fir1);
AudioConnection_F32          patchCord8(FMDetector1, 0, audioOutI2S1, 0);
AudioConnection_F32          patchCord9(FMDetector1, 0, audioOutI2S1, 1);
AudioConnection_F32          patchCordA(FMDetector1, 0, toneDet1, 0);
// AudioConnection_F32          patchCord10(FMDetector1, 0, recordQueue1, 0);
AudioConnection_F32          patchCord11(fir1, FMDetector1);
AudioConnection_F32          patchCord12(modulator1, 0, mixer4_2, 0);
AudioConnection_F32          patchCord13(modulator1, 0, rms1, 0);
AudioControlSGTL5000     sgtl5000_1;     //xy=157,796

// #define  SAMPLE_RATE  44117.0f
// #define  DETECTOR_TIME  300.0f
// #define  NWINDOW  (uint16_t)( 0.5 + SAMPLE_RATE * DETECTOR_TIME / 32000.0f )

void setup() {
  Serial.begin(300);  // Any value
  delay(1000);
  Serial.println("OpenAudio_ArduinoLibrary  - Full FM Test CTCSS Tone Detector");
  
  // AudioMemory(5);
  AudioMemory_F32(50, audio_settings);
  sgtl5000_1.enable();

  // NBFM Transmitter:
  sine1.frequency(CTCSSFreq);
  sine1.amplitude(0.75f);             // CTCSS tone 750 Hz deviation (15% of 5000)
  noiseWhite1.amplitude(2.0f);        // RMS 2000 Hz deviation, 1 sigma
  modulator1.setFMScale(1000.0f);     // Sine wave ampl=1.0 is now 1 kHz dev
  // Bandpass the noise a bit to make it imitate voice, grossly!
  biQuad1.setBandpass(0, 800.0f, 4.0f);  // (uint32_t stage, float frequency, float q)
  biQuad1.begin();
  //                             (_doAM, _doPM, _doFM, _bothIQ)
  modulator1.doModulation_AM_PM_FM(false, false, true, false);  
  modulator1.frequency(15000.0f);  // Carrier frequency
  modulator1.amplitude(0.01f);     // Set in loop below

  // NBFM Receiver:
  noiseWhite2.amplitude(0.01f);        // Receiver noise arbitrary level here
  fir1.begin(firFM_BPF, 82, 128);      // NBFM I-F filter
  // The FM detector has error checking during object construction
  // when Serial.print is not available.  See RadioFMDetector_F32.h:
  Serial.print("FM Initialization errors: ");
  Serial.println( FMDetector1.returnInitializeFMError() );
  // FMDetector1.setSquelchThreshold(0.7f);
  FMDetector1.frequency(15000.0f);
  // recordQueue1.begin();

  // Sub-audible tone detector:
  // CTCSS ranges from 67 to 254 Hz
  // Actual CTCSS use  seems to be 77.0 to 203.5 Hz
  toneDet1.initCTCSS();
  toneDet1.frequency(CTCSSFreq);    // or (CTCSSFreq, 300.0);
  toneDet1.thresholds(0.0f, 0.4f);

  delay(500);
  Serial.println(waitTone2());  // Just to load filters
  modulator1.amplitude(0.01);
  measureDataPoint();
  
  Serial.print("\nCTCSS Freq = "); Serial.println(CTCSSFreq);
  Serial.println("\n   pTone      pRef    pTone/pRef  pSigdB  pNoisedB  S/N dB");
  
  for(float sig=0.00316228; sig<0.158114; sig*=1.04)
    {
    modulator1.amplitude(sig);
    measureDataPoint();
    }
  }

void loop() {
//  measureDataPoint();
  }

/*
   while (!rms2.available())  ;
   float pNoise = 20.0f*log10f(rms2.read());
   while (!rms2.available())  ;
   pNoise = 20.0f*log10f(rms2.read());
   while (!rms2.available())  ;
   pNoise = 20.0f*log10f(rms2.read());
   Serial.print("FM Det out (dB) = ");
   Serial.println(pNoise, 3);
 */

#ifdef OUTPUT_QUEUE
  if( recordQueue1.available() )
    { pq = recordQueue1.readBuffer();
      for(int i=0; i<128; i++)
         Serial.println(*(pq + i),7);
      recordQueue1.freeBuffer();
    }
#endif


void measureDataPoint(void) {
   if(!waitTone2())  {Serial.println("No tone output"); return;}
   float pt = toneDet1.readTonePower();
   float pr = toneDet1.readRefPower();
   Serial.print(pt, 9);
   Serial.print(", ");
   // Serial.print(10.0f*log10f(toneDet1.readRefPower()), 3);
   Serial.print(pr, 9);
   Serial.print(", ");
   Serial.print(pt/pr, 7);
   Serial.print(", ");
   while(!rms1.available() || !rms2.available() ) Serial.print(rms2.available() );
   float pSig   = 20.0f*log10f(rms1.read());
   float pNoise = 20.0f*log10f(rms2.read());
   Serial.print(pSig, 3);
   Serial.print(", ");
   Serial.print(pNoise, 3);
   Serial.print(", ");
   Serial.println(pSig - pNoise, 3);
   }

bool waitTone2(void)
  {
  unsigned long int t;
  t=micros();
  while(1) {
    if(toneDet1.available())  return true;
    if( (micros()-t) > 1000000UL) return false;
    }
  }
