/*
 * BFSK.ino  Test the BFSK  at 1200 baud with repeated data.
 * The Serial Monitor should print:
OpenAudio_ArduinoLibrary  - Test BFSK
Resulting audio samples per data bit = 40
0The quick brown fox jumped...
1The quick brown fox jumped...
2The quick brown fox jumped...
3The quick brown fox jumped...
4The quick brown fox jumped...
Data send and receive complete
 *
 * F32 library
 * Bob Larkin 5 June 2022
 * Public Domain
 */

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include <Audio.h>

float* pDat = NULL;
float32_t fa, fb, delf, dAve;  // For sweep
struct uartData* pData;
float32_t inFIRCoef[200];
float32_t inFIRadb[100];
float32_t inFIRData[528];
float32_t inFIRrdb[500];

// LPF FIR for 1200 baud
static float32_t LPF_FIR_Sinc[40] = {
0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f,
0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f,
0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f,
0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f,
0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f};

// The next needs to be 128 + the size of the FIR coefficient array
float32_t FIRbuffer[128+40];

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
RadioFMDiscriminator_F32  fmDet1(audio_settings);
UART_F32                  uart1(audio_settings);
AudioAnalyzeRMS_F32       rms1;
AudioOutputI2S_F32        audioOutI2S1(audio_settings);
AudioConnection_F32       patchCord1(modulator1, 0, mixer4_1,  0);
AudioConnection_F32       patchCord2(gwn1,       0, mixer4_1,  1);
AudioConnection_F32       patchCord4(mixer4_1,   0, inputFIR,  0);
AudioConnection_F32       patchCord5(inputFIR,   0, rms1,      0);
AudioConnection_F32       patchCord7(inputFIR,   0, fmDet1,    0);
AudioConnection_F32       patchcord8(fmDet1,     0, uart1,     0);
AudioControlSGTL5000      sgtl5000_1;

void setup() {
   uint32_t spdb;
   static float32_t snrDB = 15.0f;
   static uint16_t dm0;
   static uint32_t nn, ii;
   static char ch[32];
   static uint32_t t = 0UL;

   Serial.begin(300);   // Any value, it is not used
   delay(1000);
   Serial.println("OpenAudio_ArduinoLibrary  - Test BFSK");

   AudioMemory_F32(30, audio_settings);
   // Enable the audio shield, select input, and enable output
   sgtl5000_1.enable();                   //start the audio board
   spdb = modulator1.setBFSK(1200.0f, 10, 1200.0f,   2200.0f);
   modulator1.setLPF(NULL, NULL, 0);    // No LPF
   modulator1.amplitude(1.00f);
   Serial.print("Resulting audio samples per data bit = ");
   Serial.println(spdb);

   mixer4_1.gain(0, 1.0f);  // Modulator in
   mixer4_1.gain(1, 1.0f);  // Gaussian noise in

   // Design a bandpass filter to limit the input to the FM discriminator
   for(int jj=0;  jj<12;   jj++)   inFIRadb[jj] = -100.0f;
   for(int jj=3;  jj<=11;  jj++)   inFIRadb[jj] = 0.0f;
   for(int jj=12; jj<100;  jj++)   inFIRadb[jj] = -100.0f;
   inputFIR.FIRGeneralNew(inFIRadb, 200, inFIRCoef, 40.0f, inFIRData);

   fmDet1.filterOutFIR(LPF_FIR_Sinc, 40, FIRbuffer, 0.99f); // Precede initialize
   fmDet1.initializeFMDiscriminator(1100.0f, 2350.0f, 2.0f, 3.0f);
   uart1.setUART(40, 20, 8, PARITY_NONE, 1);

   // See BFSK_random.ino for details of S/N measurement
   //   Signal power = 470.831299, 5625
   //   Noise power = 471.335632, 5625
   //   S/N in dB for S set to 0.414476 and N set to 1.0f: -0.0046
   // S/N=7 dB marginal but S/N=14 dB is solid
   snrDB = 14.0f;
   modulator1.amplitude(pow(10.0, 0.05f*(snrDB-7.65f)));
   gwn1.amplitude(1.0f);

   // Send a little data, five of these:
   // index ii 0         10        20        30
   //          v         v         v         v
   strcpy(ch, "0The quick brown fox jumped...\n"); // 32 char including ending 0
   ii = 0;    nn = 0;
   modulator1.bufferClear();
   delay(40);

   // Get UART synced up
   if( modulator1.bufferHasSpace() )
      modulator1.sendData(0X200);   // 0X00
   if( modulator1.bufferHasSpace() )
      modulator1.sendData(0X200);

   while(nn < 5)
      {
      if( modulator1.bufferHasSpace() )
         {
         // Serial.print("Send"); Serial.println((char)ch[ii]);
         dm0 = (uint16_t)ch[ii++];
         if(ii>30)   // Sends all including \n, but not the string zero.
            {
            ii=0;
            nn++;
            ch[0]++;    // Left hand character, 0 to 4
            }
         modulator1.sendData(0X200 | (dm0 << 1));  // Format ASCII to 8N1 serial
         }
      if(uart1.getNDataBuffer() > 0L)
         {
         pData = uart1.readUartData();
         Serial.print((char)pData->data);
         }
      }

   // Receive takes longer than transmit, so wait for the rest.
   t = millis();
   while((millis() - t) < 2000UL)    // Wait few seconds...
      {
      if(uart1.getNDataBuffer() > 0L)
         {
         pData = uart1.readUartData();
         Serial.print((char)pData->data);
         }
      }
   Serial.println("Data send and receive complete");
   }

void loop() {
   }


