// FT8Receive.ino  13 Oct 2022  Bob Larkin, W7PUA
// Simple command-line reception of WSJT FT8 signals for
// amateur radio.
// Added azimuth calculation.  Bob 14 Nov 2022

/*
 * Huge thanks to Charley Hill, W5BAA, for his Pocket FT8 work, much of which
 * is the basis for this INO: https://github.com/Rotron/Pocket-FT8
 * That work started from the "FT8 Decoding Library" by
 * Karlis Goba: https://github.com/kgoba/ft8_lib and thank you to both
 * contributors.
 */

/*
 * See Examples/Teensy/TimeTeensy3.ino for
 * example code illustrating Time library with Real Time Clock.
 */

/* This INO uses the Teensy F32 Audio Library interface to test
 * the radioFT8Demodulator_F32 data collector.  This should
 * not be used with "#define W5BAA_INTERFACE" in the file
 * radioFT8Demodulator_F32.h, as that interface is different.
 */

/* The F32 Audio library class, radioFT8Demodulator_F32, performs the
 * data collection and organization for FT8 reception.  This occurs
 * after every 128 audio samples, automatically.  The sync and decode
 * functions are time intensive and therefore not done with the audio
 * interrupts.  A complete set of Karlis Goba FT8 reception files is
 * included with this INO and have been slightly renamed with an "R"
 * at the end.  The Goba functions have stayed the same and minimal
 * changes are made to the functions.
 *
 * IMPORTANT -When one wants to build a full transmit and receive
 * FT8 INO, maybe with waterfall, they should go back to the Goba and
 * Hill files referenced above.  This INO is intended as a minimal demonstration
 * of FT8 reception with emphasis on testing the radioFT8Demodulator_F32
 * class.   That said, be aware that these receive  files include
 * snr estimation not available from the other sets.
 */

#include "Arduino.h"
#include <TimeLib.h>
#include <OpenAudio_ArduinoLibrary.h>   // Added for F32 Teensy library RSL
#include <Audio.h>
#include "AudioStream.h"
#include "arm_math.h"

// The following debugging print dumps are available:
// DEBUG1  - Main INO and overall control
// DEBUG_N - Noise measurement data for snr estimates
// DEBUG_D - Decode measurements
// Uncomment the following for debugging:
// #define DEBUG1
// #define DEBUG_N
// #define DEBUG_D

#define FFT_SIZE  2048
#define block_size 128
#define input_gulp_size 1024
#define HIGH_FREQ_INDEX 368
#define LOW_FREQ_INDEX 48
#define FT8_FREQ_SPACING 6.25  //                           ??
#define ft8_min_freq FT8_FREQ_SPACING * LOW_FREQ_INDEX
#define ft8_msg_samples 92

// Define FT8 symbol counts
#define FT8_ND  58
#define FT8_NN  79

// Define the LDPC sizes
#define FT8_N   174
#define FT8_K   91
#define FT8_M   83
#define FT8_K_BYTES  12

// A couple of prototypes to help the compile order
void init_DSP(void);
void extract_power(int);

// Alternate interface format.
// #define W5BAA

// Only 48 and 96 kHz audio sample rates are currently supported.
const float32_t sample_rate_Hz = 48000.0f;
const int       audio_block_samples = 128;
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

// NOTE Changed class name to start with capital "R"  RSL 7 Nov 2022

AudioInputI2S_F32        audioInI2S1(audio_settings);    //xy=100,150
AudioEffectGain_F32      gain1;          //xy=250,150
AudioAnalyzePeak_F32     peak1;          //xy=400,250
RadioFT8Demodulator_F32  demod1;         //xy=400,150
AudioConnection_F32      patchCord1(audioInI2S1, 0, gain1, 0);
AudioConnection_F32      patchCord2(gain1, demod1);
AudioConnection_F32      patchCord3(gain1, peak1);
AudioControlSGTL5000     sgtl5000_1;     //xy=100,250

// Communicate amongst decode functions:
typedef struct Candidate {
    int16_t      score;
    int16_t      time_offset;
    int16_t      freq_offset;
    uint8_t      time_sub;
    uint8_t      freq_sub;
	uint8_t      alt;         // Added for convenience Teensy, RSL
	float32_t    syncPower;   // Added for Teensy, RSL
} Candidate;

// This is the big file of log powers
uint8_t export_fft_power[ft8_msg_samples*HIGH_FREQ_INDEX*4] ;
// Pointer to 2048 float data for FFT array in radioDemodulator_F32
float32_t* pData2K = NULL;

float32_t FT8noisePowerEstimateL = 0.0f;  //  Works when big signals are absent
int16_t   FT8noisePwrDBIntL = 0;
float32_t FT8noisePowerEstimateH = 0.0f;  //  Works for big signals and QRMt
int16_t   FT8noisePwrDBIntH = 0;
float32_t FT8noisePeakAveRatio = 0.0f;    // > about 100 for big sigs

char Station_Call[11];  // six character call sign + /0
char home_Locator[11];  // four character locator  + /0
char Locator[11]; // four character locator  + /0
uint8_t ft8_hours, ft8_minutes, ft8_seconds;
//From gen_ft8.cpp, i.e., also used for transmit:
char Target_Call[7]; //six character call sign + /0
char Target_Locator[5]; // four character locator  + /0
int Target_RSL; // four character RSL  + /0
float32_t Station_Latitude, Station_Longitude;
float32_t Target_Latitude, Target_Longitude;

// rcvFT8State
#define FT8_RCV_IDLE 0
#define FT8_RCV_DATA_COLLECT 1
#define FT8_RCV_FIND_POWERS 2
#define FT8_RCV_READY_DECODE 3
#define FT8_RCV_DECODE 4
int rcvFT8State = FT8_RCV_IDLE;

int offset_step;
int32_t current_time, start_time;
int32_t ft8_time, ft8_mod_time, ft8_mod_time_last;
int32_t tOffset = 0;  // Added Sept 22
uint8_t secLast = 0;
const int ledPin = 13;
bool showPower = false;
bool noiseMeasured = false;
uint32_t tp = 0;
uint32_t tu = 0;

void setup(void) {
   strcpy(Station_Call, "W7PUA");
   strcpy(home_Locator, "CN84");
   Station_Latitude = mh2latf(home_Locator);
   Station_Longitude = mh2lonf(home_Locator);
   // set the Time library to use Teensy 4.x's RTC to keep time
   setSyncProvider(getTeensy3Time);
   AudioMemory_F32(50, audio_settings);
   Serial.begin(9600);
   delay(1000);
   // Enable the audio shield, select input, and enable output
   sgtl5000_1.enable();
   sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
#ifndef W5BAA
   pData2K = demod1.getDataPtr();  // 2048 floats in radioFT8Demodulator_F32
#endif
   demod1.initialize();
   demod1.setSampleRate_Hz(48000.0f);
   init_DSP();
   gain1.setGain(1.0);
   update_synchronization();
   Serial.println("FT8 Receive test");
#ifdef DEBUG1
   Serial.print("Power array bytes = ");
   Serial.println(ft8_msg_samples*HIGH_FREQ_INDEX*4);
#endif
   if (timeStatus()!= timeSet)
      Serial.println("Unable to sync with the RTC");
   else
      Serial.println("RTC has set the system time");
   //demod1.startDataCollect();   NOT FOR W5BAA interface
   }

void loop(void)  {
   int16_t inCmd; 
   int master_offset;



   if( Serial.available() )
      {
      inCmd = Serial.read();
      if(inCmd=='=')  //Set minute clock to zero
         {

         }
      else if(inCmd=='p' || inCmd=='P')  // Increase clock 0.1 sec
         {
         tOffset += 100;
         Serial.println("Increase clock 0.1 sec");
         Serial.println(0.001*(float)tOffset);
         }
      else if(inCmd=='l' || inCmd=='L')  // Decrease clock 0.1 sec
         {
         tOffset -= 100;
         Serial.println("Decrease clock 0.1 sec");
         Serial.println(0.001*(float)tOffset);
         }
      else if(inCmd=='-')  // Increase clock 1 sec
         {
         tOffset += 1000;
         Serial.println("Increase clock 1 sec");
         Serial.println(0.001*(float)tOffset);
         }
      else if(inCmd==',')  // Decrease clock 1 sec
         {
         tOffset -= 1000;
         Serial.println("Decrease clock 1 sec");
         Serial.println(0.001*(float)tOffset);
         }
      else if(inCmd=='c' || inCmd=='C')  // Clock display
         {
         Serial.print("Time Offset, millisecods = ");
         Serial.println(tOffset);
         }
      else if(inCmd=='e' || inCmd=='E')  // Toggle power display
         {
         showPower = !showPower;
         Serial.print("Show Power = ");
         Serial.println(showPower);
         }
      else if(inCmd=='?')
         {
         //Serial.println("=          Set local FT-8 clock to 0 (60 sec range).");
         Serial.println("p, P       Increase local FT8 clock by 0.1 sec");
         Serial.println("l, L       Decrease local FT8 clock by 0.1 sec");
         Serial.println("-          Increase local FT8 clock by 1 sec");
         Serial.println(",          Decrease local FT8 clock by 1 sec");
         Serial.println("c, C       Display offset");
         Serial.println("e, E       Display received power");
         Serial.println("?          Help Display (this message)");
         }
      else if(inCmd>35)   // Ignore anything below '#'
         Serial.println("Cmd ???");
      }   //  End, if Serial Available

   // Print average power level to Serial Monitor
   // Useful for testing and synchronizing the FT8 clock. Shows total band power.
   if( showPower && demod1.powerAvailable() )
      {
      float32_t pwr=demod1.powerRead();
      float32_t fl;
      if(second()>secLast || (second()==0 && secLast==59))
         {
         secLast = second();
         tp = millis();
         }
      fl = millis() + tOffset  - start_time;
      while(fl>=15000.0f)
         fl -= 15000.0f;
      Serial.print(0.001f*fl);
      Serial.print(" ");

      //   Serial.print(0.001*(millis() - tp)+(float)second()); Serial.print(" ");
      Serial.print(pwr); Serial.print(" ");
      for(int jj=0; jj<2*(30-(int)(-0.5*pwr)); jj++)
         Serial.print("*");
      Serial.println();
      }

   update_synchronization();
   ft8_mod_time = ft8_time%15000;
   if(  ft8_mod_time<100  &&  ft8_mod_time_last>14900 )
      {
      ft8_mod_time_last = ft8_mod_time;
      rcvFT8State = FT8_RCV_DATA_COLLECT;
      demod1.startDataCollect();  // Turn on decimation and data
#ifdef DEBUG1
      Serial.println("= = = = = SYNC TIME 15 = = = = =");
#endif
      digitalWrite(ledPin, HIGH);   // set the LED on
      delay(100);
      digitalWrite(ledPin, LOW);   // set the LED on
      }
   else
      ft8_mod_time_last = ft8_mod_time;

   if(rcvFT8State==FT8_RCV_DATA_COLLECT && demod1.available())
      {
	  // Note: The RadioFT8Demodulator provides at least 2.7 milliseconds, after data is
      // available, before pData2K is written over. The extract_power() here must stay
      // on schedule to protect the data.  THis should be OK.
      // Here every 80 mSec for FFT
      rcvFT8State = FT8_RCV_FIND_POWERS;
      // 1472 * 92 = 135424
      //master_offset = offset_step * FT_8_counter; //offset_step=1472
      master_offset = 736 * (demod1.getFFTCount() -1);
#ifdef DEBUG1
      Serial.print("master offset = "); Serial.println(master_offset);
      Serial.print("Extract Power, fft count = "); Serial.println( demod1.getFFTCount() );
#endif
      extract_power(master_offset);    // Do FFT and log powers
	  if(demod1.getFFTCount() >= 184)
		 rcvFT8State = FT8_RCV_READY_DECODE;
	  else
         rcvFT8State = FT8_RCV_DATA_COLLECT;
#ifdef DEBUG1
      Serial.println("Power array updated");
#endif
      }

   if(rcvFT8State==FT8_RCV_READY_DECODE )
      {
      rcvFT8State = FT8_RCV_DECODE;
#ifdef DEBUG1
      Serial.println("FT8 Decode");
#endif
      tu = micros();
      ft8_decode();
	  printFT8Received();
      rcvFT8State = FT8_RCV_IDLE;
#ifdef DEBUG1
      Serial.print("ft8_decode Time, uSec = ");
      Serial.println(micros() - tu);
#endif
      }
   delay(1);   // Don't increase
   }   // End loop()

time_t getTeensy3Time()  {
    return Teensy3Clock.get();
   }

// This starts the receiving data collection every 15 sec
void update_synchronization() {
   int32_t hours_fraction;
   
   current_time = millis() + tOffset;
   ft8_time = current_time  - start_time;
   ft8_hours =  (int8_t)(ft8_time/3600000);
   hours_fraction = ft8_time % 3600000;
   ft8_minutes = (int8_t)  (hours_fraction/60000);
   ft8_seconds = (int8_t)((hours_fraction % 60000)/1000);
   }
