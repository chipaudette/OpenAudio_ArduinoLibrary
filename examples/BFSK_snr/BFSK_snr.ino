/*
 * BFSK_snr.ino  Measure the BFSK  at 1200 baud to determine
 * power signal/noise ratio after band-pass filtering .
 * F32 library
 * Bob Larkin 8 June 2022
 * Public Domain -
 */
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include <Audio.h>

int numberSamples = 0;
float* pDat = NULL;
float32_t fa, fb, delf, dAve;  // For sweep
struct uartData* pData;
uint32_t nn, errorCount, errorCountFrame;
// Storage for the input BPF FIR filter
float32_t inFIRCoef[200];
float32_t inFIRadb[100];
float32_t inFIRData[528];
float32_t inFIRrdb[500];  // To calculate response

// T3.x supported sample rates: 2000, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 44117, 48000,
//                             88200, 88235 (44117*2), 95680, 96000, 176400, 176470, 192000
// T4.x supports any sample rate the codec will handle.
const float sample_rate_Hz = 48000.0f ;  // 24000, 44117, or other frequencies listed above (untested)
const int   audio_block_samples = 128;   // Others untested
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);  // Not used

RadioBFSKModulator_F32    modulator1(audio_settings);
AudioSynthGaussian_F32    gwn1;
AudioMixer4_F32           mixer4_1;
AudioFilterFIRGeneral_F32 inputFIR;
AudioAnalyzeRMS_F32       rms1;
AudioOutputI2S_F32        audioOutI2S1(audio_settings);
AudioConnection_F32       patchCord1(modulator1, 0, mixer4_1,  0);
AudioConnection_F32       patchCord2(gwn1,       0, mixer4_1,  1);
AudioConnection_F32       patchCord4(mixer4_1,   0, inputFIR,  0);
AudioConnection_F32       patchCord5(inputFIR,   0, rms1,      0);
AudioControlSGTL5000      sgtl5000_1;

void setup() {
   uint32_t spdb;
   float32_t pSig, pNoise;
   uint32_t tt, nMeas;

   Serial.begin(300);   // Any value, it is not used
   delay(1000);
   Serial.println("OpenAudio_ArduinoLibrary  - Test BFSK");
   Serial.println("Statistics with a Fixed Bit Pattern");

   AudioMemory_F32(30, audio_settings);
   sgtl5000_1.enable();                   //start the audio board (NU)

   modulator1.setLPF(NULL, NULL, 0);    // No LPF
   modulator1.amplitude(1.00f);
   spdb = modulator1.setBFSK(1200.0f, 10, 1200.0f,   2200.0f);
   Serial.print("Audio samples per data bit = ");
   Serial.println(spdb);

   gwn1.amplitude(0.5f);
   mixer4_1.gain(0, 1.0f);  // Modulator in
   mixer4_1.gain(1, 1.0f);  // Gaussian noise in

   // Design a bandpass filter to limit the input to the FM discriminator
   for(int jj=0;  jj<12;   jj++)   inFIRadb[jj] = -100.0f;
   for(int jj=3;  jj<=11;  jj++)   inFIRadb[jj] = 0.0f;
   for(int jj=12; jj<100;  jj++)   inFIRadb[jj] = -100.0f;
   inputFIR.FIRGeneralNew(inFIRadb, 200, inFIRCoef, 40.0f, inFIRData);

   // This measures S/N.  Set amplitudes and turn on either signal
   // or noise using mixer gain.  Next we set the signal and noise
   // amplitudes.  The pow() equation allows us to enter the S/N directly.
   //                          S/N in dB --v
   modulator1.amplitude(pow(10.0, 0.05f*(0.00f-7.65f)));
   gwn1.amplitude(1.0f);

   // Reversing the process, we directly measure the S/N at
   // the output of the BPF, going to the discriminator input.
   mixer4_1.gain(0, 1.0f);  // Modulator in
   mixer4_1.gain(1, 0.0f);  // No Gaussian noise in
   pingTransmit();  pingTransmit();  pingTransmit();
   getPower();
   nMeas=0;
   tt = millis();
   pSig = 0.0f;
   pNoise = 0.0f;
   while(millis()-tt < 15000)   // 15 second average
      {
      pingTransmit();  // Send random data if there is room
      float32_t pp = getPower();
      if(pp >= 0.0)
         {
         pSig += pp;
         nMeas++;
         }
      }
   Serial.print("Signal power = ");
   Serial.print(pSig, 6);  Serial.print(", ");   Serial.println(nMeas);

   // Now repeat using noise, but no signal:
   mixer4_1.gain(0, 0.0f);  // No Modulator in
   mixer4_1.gain(1, 1.0f);  // Gaussian noise in
   getPower();
   nMeas=0;
   tt = millis();
   while(millis()-tt < 15000)   // 15 second average
      {
      float32_t pp = getPower();
      if(pp >= 0.0)
         {
         pNoise += pp;
         nMeas++;
         }
      }
   Serial.print("Noise power = ");
   Serial.print(pNoise, 6);  Serial.print(", ");   Serial.println(nMeas);
   Serial.print("S/N in dB for S set to 0.414476 and N set to 1.0f: ");
   Serial.println(10.0*log10f(pSig/pNoise), 4);
   // This code is omitted by #if 0, as it need not normally run.
   // Results from running this code:
   //   Signal power = 470.831299, 5625
   //   Noise power = 471.335632, 5625
   //   S/N in dB for S set to 0.414476 and N set to 1.0f: -0.0046
   }

void loop() {
   }

// Used to calibrate S/N
float32_t getPower(void)  {
   float32_t p;
   if(rms1.available())
      {
      p = rms1.read();
      p *= p;
      return p;
      }
   else
      return -1.0f;
   }

// The transmitter has a buffer and we can keep the tranmission
// running continuosly
void pingTransmit(void)  {
   if( modulator1.bufferHasSpace() )
      modulator1.sendData(0X200 | (random(255) << 1));
   }
