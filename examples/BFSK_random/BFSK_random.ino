/*
 * BFSK_random.ino  Test the BFSK  at 1200 baud with random data
 * to determine byte error rate.   Vary S/N.   A slow process.
 * F32 Teensy Audio Librarylibrary
 * Bob Larkin 8 June 2022, Rev 15 June 2022
 * Public Domain
 */
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include <Audio.h>


// Uncomment to see frequency response of input BPF:
// #define PRINT_BPF_FREQ_RESPONSE

int numberSamples = 0;
float* pDat = NULL;
float32_t fa, fb, delf, dAve;  // For sweep
struct uartData* pData;
uint32_t errorCount, errorCountFrame;
float32_t inFIRCoef[200];
float32_t inFIRadb[100];
float32_t inFIRData[528];
float32_t inFIRrdb[500];

// A data storage FIFO for send data
float32_t xmitData[128];
int64_t indexIn = 0ULL;
// Correlation data
float32_t xcor[128];

// LPF FIR for 1200 baud
static float32_t LPF_FIR_Sinc[40] = {
0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f,
0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f,
0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f,
0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f,
0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f, 0.025f};

float32_t LPF_FIR_State[128 + 40];

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
   static uint16_t dm0;
   static uint32_t nn;

   Serial.begin(300);   // Any value, it is not used
   delay(1000);
   Serial.println("OpenAudio_ArduinoLibrary  - Test BFSK");
   Serial.println("Byte error statistics with a random bit pattern.");
   delay(1000);
   AudioMemory_F32(30, audio_settings);
   // Enable the audio shield, select input, and enable output
   sgtl5000_1.enable();                   //start the audio board
   sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);       // or AUDIO_INPUT_MIC
   modulator1.setLPF(NULL, NULL, 0);    // No LPF
   spdb = modulator1.setBFSK(1200.0f, 10, 1200.0f,   2200.0f);
   modulator1.amplitude(1.00f);
   Serial.print("Resulting audio samples per data bit = ");
   Serial.println(spdb);

   gwn1.amplitude(0.5f);    // Set S/N
   mixer4_1.gain(0, 1.0f);  // Modulator in
   mixer4_1.gain(1, 1.0f);  // Gaussian noise in

   // Design a bandpass filter to limit the input to the FM discriminator
   for(int jj=0;  jj<12;   jj++)   inFIRadb[jj] = -100.0f;
   for(int jj=3;  jj<=11;  jj++)   inFIRadb[jj] = 0.0f;
   for(int jj=12; jj<100;  jj++)   inFIRadb[jj] = -100.0f;
   inputFIR.FIRGeneralNew(inFIRadb, 200, inFIRCoef, 40.0f, inFIRData);

#ifdef PRINT_BPF_FREQ_RESPONSE
   // Gather the data for a plot of the response.  Output goes to Serial Monitor.
   // I use highlighting and Ctrl-C to get the data for plotting.
   Serial.println("\nResponse of Bandpass Filter ahead of the Discriminator in dB:");
   inputFIR.getResponse(500, inFIRrdb);
   for(int jj =0; jj<500; jj++)
      {
      Serial.print(48.0f * (int)jj); Serial.print(",");  // Frequency, Hz
      Serial.println(inFIRrdb[jj]);      // Respnse in dB
      }
   Serial.println("----------------------------");
#endif

   fmDet1.filterOutFIR(LPF_FIR_Sinc, 40, LPF_FIR_State, 0.99f);
   fmDet1.initializeFMDiscriminator(1100.0f, 2350.0f, 2.0f, 3.0f);
   uart1.setUART(40, 20, 8, PARITY_NONE, 1);

   // Next we set the signal and noise
   // amplitudes.  The pow() equation allows us to enter the S/N directly.
   //                          S/N in dB --v
   modulator1.amplitude(pow(10.0, 0.05f*(0.00f-7.65f)));
   gwn1.amplitude(1.0f);   // Noise fixed, vary signal level
   // See BFSKsnr.ino for details

   // We can now evaluate the performance of the transmitter and receiver
   // by varying the S/N and counting the number of data errors.  The data
   // will be set randomly over all 8 data bits.
   // Thus we can compute error levels vs S/N in dB
   for(float32_t snrDB=4.0f; snrDB<=11.0f; snrDB+=0.5f)
   //for(float32_t snrDB=11.0f; snrDB<=13.5f; snrDB+=0.5f)   // Use with nn=100000
      {
      modulator1.amplitude(pow(10.0f, 0.05f*(snrDB-7.65f)));
      nn = 0;
      errorCount = 0;

      //while(nn<100000 && errorCount<1000)  // Use for S/N > 11 dB
      while(nn<10000 && errorCount<1000)
         {
         if( modulator1.bufferHasSpace() )
            {
            dm0 = random(255);                 // Serial.println(dm0);
            // Save a copy of sent data in circular buffer
            xmitData[indexIn & 0X7F] = (float32_t)dm0;
            indexIn++;
            modulator1.sendData(0X200 | (dm0 << 1));
            nn++;
            }
         if(uart1.getNDataBuffer() > 0)
            {
            pData = uart1.readUartData();   // Pointer to data structure

            if( pData->data!=xmitData[(indexIn-65LL) & 0X7F] &&
                pData->data!=xmitData[(indexIn-66LL) & 0X7F] )
               {
               errorCount++;
               }
            }
         }      // End, waiting for enough data
      Serial.print("S/N= "); Serial.print(snrDB, 3);
      Serial.print(", number= "); Serial.print(nn);
      Serial.print(", errors= "); Serial.println(errorCount);
      }
   }

void loop() {
   }
