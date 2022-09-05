/*
 *  New FT8Transmit.ino    RSL 29 Aug 2022
 *
 * Generates a subset of possible FT8 messages.
 * S/N is controlled accurately.  Uses Teensy Floating Point
 * OpenAudio_TeensyLibrary.
 * See radioFT8Modulator_F32 for information on the implementation.
 * This INO is in the public domain.
 */

#include "Arduino.h"
#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"
#include <Audio.h>

// Only 48 and 96 kHz audio sample rates are currently supported.
const float sample_rate_Hz = 48000.0f;
const int   audio_block_samples = 128;
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

AudioSynthGaussian_F32       GaussianWhiteNoise1(audio_settings); //xy=101,148
RadioFT8Modulator_F32        FT8Mod1;                             //xy=117,277
AudioMixer4_F32              mixer4_1;                            //xy=282,218
AudioFilterFIRGeneral_F32    filterFIRgeneral1(audio_settings);   //xy=452,181
AudioOutputI2S_F32           audioOutI2S1(audio_settings);        //xy=532,243
AudioAnalyzeRMS_F32          rms1;                                //xy=625,180
AudioConnection_F32          patchCord1(GaussianWhiteNoise1, 0, mixer4_1, 0);
AudioConnection_F32          patchCord2(FT8Mod1, 0, mixer4_1, 1);
AudioConnection_F32          patchCord5(mixer4_1, filterFIRgeneral1);
AudioConnection_F32          patchCord3(filterFIRgeneral1, 0, audioOutI2S1, 0);
AudioConnection_F32          patchCord4(filterFIRgeneral1, 0, audioOutI2S1, 1);
AudioConnection_F32          patchCord6(filterFIRgeneral1, rms1);
AudioControlSGTL5000         sgtl5000_1;                          //xy=357,318

/* FIR filter to measure noise power in a 2500 Hz BW
 * Designed with http://t-filter.appspot.com
 * Sampling frequency: 48000 Hz
 * 0 Hz to 100 Hz and 3000 Hz to 24000 Hz  Attenuation = 60.031 dB
 * 330 Hz to 2770 Hz   Ripple = 0.073 dB
 */
static float32_t filter2500BP[597] = {
-0.0005125820f,-0.0000202947f, 0.0000003593f, 0.0000329934f, 0.0000736751f, 0.0001169578f,
 0.0001564562f, 0.0001855962f, 0.0001984728f, 0.0001906737f, 0.0001599822f, 0.0001068279f,
 0.0000344273f,-0.0000514436f,-0.0001430419f,-0.0002315220f,-0.0003080131f,-0.0003647587f,
-0.0003961436f,-0.0003995218f,-0.0003756478f,-0.0003287024f,-0.0002658396f,-0.0001963221f,
-0.0001303695f,-0.0000777464f,-0.0000465332f,-0.0000418429f,-0.0000651581f,-0.0001139975f,
-0.0001821358f,-0.0002604090f,-0.0003379262f,-0.0004035583f,-0.0004475075f,-0.0004627523f,
-0.0004461496f,-0.0003990178f,-0.0003271062f,-0.0002399160f,-0.0001495029f,-0.0000688527f,
-0.0000100068f, 0.0000176282f, 0.0000087805f,-0.0000368277f,-0.0001142343f,-0.0002137524f,
-0.0003221742f,-0.0004245023f,-0.0005060412f,-0.0005544731f,-0.0005617283f,-0.0005252710f,
-0.0004486501f,-0.0003412138f,-0.0002169625f,-0.0000927250f, 0.0000141208f, 0.0000880971f,
 0.0001179086f, 0.0000982361f, 0.0000307076f,-0.0000761311f,-0.0002078708f,-0.0003460213f,
-0.0004701224f,-0.0005615004f,-0.0006049803f,-0.0005918278f,-0.0005209351f,-0.0003992998f,
-0.0002412975f,-0.0000668730f, 0.0001011003f, 0.0002399869f, 0.0003306072f, 0.0003600909f,
 0.0003239133f, 0.0002268029f, 0.0000823809f,-0.0000884979f,-0.0002604794f,-0.0004071616f,
-0.0005048299f,-0.0005359033f,-0.0004916409f,-0.0003736551f,-0.0001940102f, 0.0000261329f,
 0.0002593430f, 0.0004753909f, 0.0006458134f, 0.0007476677f, 0.0007671322f, 0.0007017674f,
 0.0005611468f, 0.0003658605f, 0.0001449418f,-0.0000679871f,-0.0002393698f,-0.0003405009f,
-0.0003517550f,-0.0002656412f,-0.0000882146f, 0.0001613566f, 0.0004531208f, 0.0007502948f,
 0.0010142494f, 0.0012099822f, 0.0013111621f, 0.0013041379f, 0.0011902347f, 0.0009859932f,
 0.0007212731f, 0.0004353657f, 0.0001717278f,-0.0000281207f,-0.0001305200f,-0.0001146943f,
 0.0000239082f, 0.0002725259f, 0.0006024022f, 0.0009724061f, 0.0013345069f, 0.0016403901f,
 0.0018483209f, 0.0019292574f, 0.0018713163f, 0.0016819017f, 0.0013872308f, 0.0010291844f,
 0.0006595979f, 0.0003334254f, 0.0001008215f,-0.0000000058f, 0.0000514030f, 0.0002543686f,
 0.0005865884f, 0.0010066897f, 0.0014595849f, 0.0018838923f, 0.0022204845f, 0.0024209305f,
 0.0024546503f, 0.0023137474f, 0.0020147314f, 0.0015968377f, 0.0011170820f, 0.0006426406f,
 0.0002415985f,-0.0000267023f,-0.0001203145f,-0.0000211214f, 0.0002619020f, 0.0006935370f,
 0.0012155342f, 0.0017563231f, 0.0022384277f, 0.0025907218f, 0.0027584970f, 0.0027114957f,
 0.0024488224f, 0.0019998882f, 0.0014211081f, 0.0007886354f, 0.0001880784f,-0.0002974170f,
-0.0005990413f,-0.0006724584f,-0.0005047619f,-0.0001171406f, 0.0004372104f, 0.0010797374f,
 0.0017169949f, 0.0022534601f, 0.0026050371f, 0.0027112878f, 0.0025446519f, 0.0021152693f,
 0.0014705327f, 0.0006894468f,-0.0001277897f,-0.0008733366f,-0.0014468246f,-0.0017698095f,
-0.0017974750f,-0.0015259241f,-0.0009938711f,-0.0002783418f, 0.0005151815f, 0.0012669280f,
 0.0018594224f, 0.0021942034f, 0.0022063046f, 0.0018744400f, 0.0012252608f, 0.0003308740f,
-0.0006999772f,-0.0017363651f,-0.0026430272f,-0.0032994372f,-0.0036172996f,-0.0035539945f,
-0.0031199655f,-0.0023787848f,-0.0014397352f,-0.0004436511f, 0.0004561031f, 0.0011150128f,
 0.0014181900f, 0.0012975421f, 0.0007428372f,-0.0001950981f,-0.0014097317f,-0.0027521917f,
-0.0040510049f,-0.0051355902f,-0.0058603728f,-0.0061261746f,-0.0058958745f,-0.0052041918f,
-0.0041474660f,-0.0028816782f,-0.0015956931f,-0.0004880525f, 0.0002612250f, 0.0005167695f,
 0.0002085432f,-0.0006559859f,-0.0019902208f,-0.0036379161f,-0.0053928385f,-0.0070261631f,
-0.0083180284f,-0.0090889780f,-0.0092268159f,-0.0087049647f,-0.0075894402f,-0.0060330006f,
-0.0042568860f,-0.0025221767f,-0.0010944561f,-0.0002064388f,-0.0000237284f,-0.0006187163f,
-0.0019563607f,-0.0038947756f,-0.0062005014f,-0.0085775775f,-0.0107065383f,-0.0122883661f,
-0.0130875874f,-0.0129684039f,-0.0119185125f,-0.0100567406f,-0.0076227165f,-0.0049491711f,
-0.0024198814f,-0.0004183923f, 0.0007258785f, 0.0007867696f,-0.0003212561f,-0.0025231429f,
-0.0055845169f,-0.0091349479f,-0.0127114002f,-0.0158169578f,-0.0179877599f,-0.0188595572f,
-0.0182248418f,-0.0160724049f,-0.0126028433f,-0.0082166040f,-0.0034743283f, 0.0009670264f,
 0.0044354922f, 0.0063335060f, 0.0062244751f, 0.0039053136f,-0.0005450570f,-0.0067487839f,
-0.0140477185f,-0.0215575093f,-0.0282527468f,-0.0330748862f,-0.0350513372f,-0.0334119094f,
-0.0276905793f,-0.0177980957f,-0.0040578279f, 0.0128017918f, 0.0316985253f, 0.0512872217f,
 0.0700815899f, 0.0865943871f, 0.0994813474f, 0.1076734313f, 0.1104831570f, 0.1076734313f,
 0.0994813474f, 0.0865943871f, 0.0700815899f, 0.0512872217f, 0.0316985253f, 0.0128017918f,
-0.0040578279f,-0.0177980957f,-0.0276905793f,-0.0334119094f,-0.0350513372f,-0.0330748862f,
-0.0282527468f,-0.0215575093f,-0.0140477185f,-0.0067487839f,-0.0005450570f, 0.0039053136f,
 0.0062244751f, 0.0063335060f, 0.0044354922f, 0.0009670264f,-0.0034743283f,-0.0082166040f,
-0.0126028433f,-0.0160724049f,-0.0182248418f,-0.0188595572f,-0.0179877599f,-0.0158169578f,
-0.0127114002f,-0.0091349479f,-0.0055845169f,-0.0025231429f,-0.0003212561f, 0.0007867696f,
 0.0007258785f,-0.0004183923f,-0.0024198814f,-0.0049491711f,-0.0076227165f,-0.0100567406f,
-0.0119185125f,-0.0129684039f,-0.0130875874f,-0.0122883661f,-0.0107065383f,-0.0085775775f,
-0.0062005014f,-0.0038947756f,-0.0019563607f,-0.0006187163f,-0.0000237284f,-0.0002064388f,
-0.0010944561f,-0.0025221767f,-0.0042568860f,-0.0060330006f,-0.0075894402f,-0.0087049647f,
-0.0092268159f,-0.0090889780f,-0.0083180284f,-0.0070261631f,-0.0053928385f,-0.0036379161f,
-0.0019902208f,-0.0006559859f, 0.0002085432f, 0.0005167695f, 0.0002612250f,-0.0004880525f,
-0.0015956931f,-0.0028816782f,-0.0041474660f,-0.0052041918f,-0.0058958745f,-0.0061261746f,
-0.0058603728f,-0.0051355902f,-0.0040510049f,-0.0027521917f,-0.0014097317f,-0.0001950981f,
 0.0007428372f, 0.0012975421f, 0.0014181900f, 0.0011150128f, 0.0004561031f,-0.0004436511f,
-0.0014397352f,-0.0023787848f,-0.0031199655f,-0.0035539945f,-0.0036172996f,-0.0032994372f,
-0.0026430272f,-0.0017363651f,-0.0006999772f, 0.0003308740f, 0.0012252608f, 0.0018744400f,
 0.0022063046f, 0.0021942034f, 0.0018594224f, 0.0012669280f, 0.0005151815f,-0.0002783418f,
-0.0009938711f,-0.0015259241f,-0.0017974750f,-0.0017698095f,-0.0014468246f,-0.0008733366f,
-0.0001277897f, 0.0006894468f, 0.0014705327f, 0.0021152693f, 0.0025446519f, 0.0027112878f,
 0.0026050371f, 0.0022534601f, 0.0017169949f, 0.0010797374f, 0.0004372104f,-0.0001171406f,
-0.0005047619f,-0.0006724584f,-0.0005990413f,-0.0002974170f, 0.0001880784f, 0.0007886354f,
 0.0014211081f, 0.0019998882f, 0.0024488224f, 0.0027114957f, 0.0027584970f, 0.0025907218f,
 0.0022384277f, 0.0017563231f, 0.0012155342f, 0.0006935370f, 0.0002619020f,-0.0000211214f,
-0.0001203145f,-0.0000267023f, 0.0002415985f, 0.0006426406f, 0.0011170820f, 0.0015968377f,
 0.0020147314f, 0.0023137474f, 0.0024546503f, 0.0024209305f, 0.0022204845f, 0.0018838923f,
 0.0014595849f, 0.0010066897f, 0.0005865884f, 0.0002543686f, 0.0000514030f,-0.0000000058f,
 0.0001008215f, 0.0003334254f, 0.0006595979f, 0.0010291844f, 0.0013872308f, 0.0016819017f,
 0.0018713163f, 0.0019292574f, 0.0018483209f, 0.0016403901f, 0.0013345069f, 0.0009724061f,
 0.0006024022f, 0.0002725259f, 0.0000239082f,-0.0001146943f,-0.0001305200f,-0.0000281207f,
 0.0001717278f, 0.0004353657f, 0.0007212731f, 0.0009859932f, 0.0011902347f, 0.0013041379f,
 0.0013111621f, 0.0012099822f, 0.0010142494f, 0.0007502948f, 0.0004531208f, 0.0001613566f,
-0.0000882146f,-0.0002656412f,-0.0003517550f,-0.0003405009f,-0.0002393698f,-0.0000679871f,
 0.0001449418f, 0.0003658605f, 0.0005611468f, 0.0007017674f, 0.0007671322f, 0.0007476677f,
 0.0006458134f, 0.0004753909f, 0.0002593430f, 0.0000261329f,-0.0001940102f,-0.0003736551f,
-0.0004916409f,-0.0005359033f,-0.0005048299f,-0.0004071616f,-0.0002604794f,-0.0000884979f,
 0.0000823809f, 0.0002268029f, 0.0003239133f, 0.0003600909f, 0.0003306072f, 0.0002399869f,
 0.0001011003f,-0.0000668730f,-0.0002412975f,-0.0003992998f,-0.0005209351f,-0.0005918278f,
-0.0006049803f,-0.0005615004f,-0.0004701224f,-0.0003460213f,-0.0002078708f,-0.0000761311f,
 0.0000307076f, 0.0000982361f, 0.0001179086f, 0.0000880971f, 0.0000141208f,-0.0000927250f,
-0.0002169625f,-0.0003412138f,-0.0004486501f,-0.0005252710f,-0.0005617283f,-0.0005544731f,
-0.0005060412f,-0.0004245023f,-0.0003221742f,-0.0002137524f,-0.0001142343f,-0.0000368277f,
 0.0000087805f, 0.0000176282f,-0.0000100068f,-0.0000688527f,-0.0001495029f,-0.0002399160f,
-0.0003271062f,-0.0003990178f,-0.0004461496f,-0.0004627523f,-0.0004475075f,-0.0004035583f,
-0.0003379262f,-0.0002604090f,-0.0001821358f,-0.0001139975f,-0.0000651581f,-0.0000418429f,
-0.0000465332f,-0.0000777464f,-0.0001303695f,-0.0001963221f,-0.0002658396f,-0.0003287024f,
-0.0003756478f,-0.0003995218f,-0.0003961436f,-0.0003647587f,-0.0003080131f,-0.0002315220f,
-0.0001430419f,-0.0000514436f, 0.0000344273f, 0.0001068279f, 0.0001599822f, 0.0001906737f,
 0.0001984728f, 0.0001855962f, 0.0001564562f, 0.0001169578f, 0.0000736751f, 0.0000329934f,
 0.0000003593f,-0.0000202947f,-0.0005125820f};

// Messages 0 to 7
const char* inputString[] = {
        "CQ LL3JG",
        "CQ LL3JG KO26",
        "L0UAA LL3JG KO26",
        "L0UAA LL3JG +02",
        "L0UAA LL3JG RRR",
        "K1ABC W9XYZ RR73",
        "TNX BOB 73 GL",
        "ABCDEFG123456"};
int32_t sendNext = -1;
float32_t pStateArray[1322];  // 2*597+128
float32_t  t60, t60Last;
uint32_t t60Base;
bool displayClock = false;
bool displayPower = false;
bool sendEvery15Sec = true;
bool doing15SecSend = false;
float32_t snrDB = 0.0f;

void setup(void) {
   Serial.begin(9600);    delay(500);
   Serial.println("FT8Transmit.ino  Ver 0.1  29 Aug 2022 W7PUA");
   Serial.println("Use '?' to list commands and '=' to zero FT8 clock.");
   Serial.println("Use 0 to 7 to set message.");

   AudioMemory_F32(20, audio_settings);

   FT8Mod1.setSampleRate_Hz(48000.0f);
   FT8Mod1.ft8Initialize();
   FT8Mod1.frequency(700.0f);
   // Calibration note: For the following = 0.1 and the GWN amplitude at 0.1,
   // the power S/N at the output of the FIR filter is 6.617 dB or
   // 4.589 as a power ratio. The filter is very close to a 2500 Hz BW
   // so this is close to the WSJT defined S/N.  Show on WSJT-X as -1.8 dB S/N.
   // (Power as used here means the RMS object output squared.)
   FT8Mod1.amplitude(0.04668);   // for 0 dB S/N.
   filterFIRgeneral1.LoadCoeffs(597, filter2500BP, pStateArray);

   GaussianWhiteNoise1.amplitude(0.1f);  // Do not change unless breaking the cal is wanted.

   sgtl5000_1.enable();

   delay(5);
   t60 = 0.000f;
   t60Last = t60;
   t60Base = millis();
   }

void loop()  {
   int16_t inCmd;
   if( Serial.available() )
      {
      inCmd = Serial.read();
      if(inCmd=='0')
         {
         sendNext = 0;
         Serial.print("Next FT8:");
         Serial.println((char*) inputString[0]);
         }
      else if(inCmd=='1')
         {
         sendNext = 1;
         Serial.print("Next FT8:");
         Serial.println((char*) inputString[1]);
         }
      else if(inCmd=='2')
         {
         sendNext = 2;
         Serial.print("Next FT8:");
         Serial.println((char*) inputString[2]);
         }
      else if(inCmd=='3')
         {
         sendNext = 3;
         Serial.print("Next FT8:");
         Serial.println((char*) inputString[3]);
         }
      else if(inCmd=='4')
         {
         sendNext = 4;
         Serial.print("Next FT8:");
         Serial.println((char*) inputString[4]);
         }
      else if(inCmd=='5')
         {
         sendNext = 5;
         Serial.print("Next FT8:");
         Serial.println((char*) inputString[5]);
         }
      else if(inCmd=='6')
         {
         sendNext = 6;
         Serial.print("Next FT8:");
         Serial.println((char*) inputString[6]);
         }
      else if(inCmd=='7')
         {
         sendNext = 7;
         Serial.print("Next FT8:");
         Serial.println((char*) inputString[7]);
         }
      else if(inCmd=='w' || inCmd=='W')  // S/N Up 1 dB
         {
         snrDB += 1.0f;
         FT8Mod1.amplitude(powf(10.0f, 0.05f*(snrDB-26.617)));   //0.04668 is amplitude for 0dB S/N
         Serial.print("SNR Up 1 dB to "); Serial.println(snrDB, 3);
         if(snrDB>23.5f)
            Serial.println("WARNING: S/N > 23 dB may cause DAC overload.");
         }

      else if(inCmd=='a' || inCmd=='A')  // S/N Down 1 dB
         {
         snrDB -= 1.0f;
         FT8Mod1.amplitude(powf(10.0f, 0.05f*(snrDB-26.617)));   //0.04668 is amplitude for 0dB S/N
         Serial.print("SNR Down 1 dB to "); Serial.println(snrDB, 3);
         }

      else if(inCmd=='t' || inCmd=='T')  // Toggle Continuous and Trigger
         {
         sendEvery15Sec = !sendEvery15Sec;
         if(sendEvery15Sec)
            Serial.println("Send every 15 sec");
         else
            Serial.println("Send triggered on Number");
         }
      else if(inCmd=='d' || inCmd=='D')  // Toggle dB Power
         {
         displayPower = !displayPower;
         if(displayPower)
            Serial.println("Display dB Power every sec");
         else
            Serial.println("No Display Power");
         }
      else if(inCmd=='=')  //Set minute clock to zero
         {
         t60 = 0;
         t60Base = millis();
         t60Last = t60;
         Serial.println("Clock Zero'd");
         }
      else if(inCmd=='p' || inCmd=='P')  // Increase clock 0.1 sec
         {
         t60 += 0.100;
         t60Base -= 100;
         Serial.println("Increase clock 0.1 sec");
         }
      else if(inCmd=='l' || inCmd=='L')  // Decrease clock 0.1 sec
         {
         t60 -= 0.100;
         t60Base += 100;
         Serial.println("Decrease clock 0.1 sec");
         }
      else if(inCmd=='-')  // Increase clock 1 sec
         {
         t60 += 1.000;
         t60Last = t60;
         t60Base -= 1000;
         Serial.println("Increase clock 1 sec");
         }
      else if(inCmd==',')  // Decrease clock 1 sec
         {
         t60 -= 1.000;
         t60Last = t60;
         t60Base -= 1000;
         Serial.println("Decrease clock 1 sec");
         }
      else if(inCmd=='c' || inCmd=='C')  // Toggle clock display
         {
         displayClock = !displayClock;
         }
      else if(inCmd=='?')
         {
         Serial.println("0, 1,...7  Select message 0 to 7");
         Serial.println("w, W       Increase S/N by 1 dB");
         Serial.println("a, A       Decrease S/N by 1 dB");
         Serial.println("t, T       Toggle Repeat every 15 sec or Trigger on msg entry");
         Serial.println("d, D       Toggle dB total power display, On or Off");
         Serial.println("=          Set local FT-8 clock to 0 (60 sec range).");
         Serial.println("p, P       Increase local FT8 clock by 0.1 sec");
         Serial.println("l, L       Decrease local FT8 clock by 0.1 sec");
         Serial.println("-          Increase local FT8 clock by 1 sec");
         Serial.println(",          Decrease local FT8 clock by 1 sec");
         Serial.println("c, C       Toggle Clock Display On or Off.");
         Serial.println("?          Help Display (this message)");
         }
      else if(inCmd>35)   // Ignore anything below '#'
         Serial.println("Cmd ???");
      }   //  End, if Serial Available

      // Update clock
      t60 = 0.001f*(float32_t)(millis() - t60Base);
      if(t60 >= 60.000f)
         {
         t60 -= 60.000f;     // Seconds
         t60Base += 60000;   // milliSeconds
         t60Last = t60;
         if(displayClock)
            {
            Serial.print("Seconds: "); Serial.println(t60, 3);
            }
         }
      if(t60-t60Last >= 1.000f)
         {
         if(displayClock)
            {
            Serial.print("Seconds: "); Serial.println(t60, 3);
            }
         if( rms1.available() && displayPower )
            {
            Serial.print("Power, dB = ");
            Serial.println(20.0*rms1.read(), 2);
            }
         t60Last += 1.000f;
         }

      // Look for immediate triggered send
      if(sendNext>=0 && !sendEvery15Sec)
         {
         FT8Mod1.cancelTransmit();
         FT8Mod1.sendData((char*) inputString[sendNext], 1, 0, 0);
         sendNext = -1;
         Serial.println("Immediate Triggered Send");
         }

      // Look for send every 15 sec
      if(!FT8Mod1.FT8TransmitBusy() && timing15() && sendNext>=0 && sendEvery15Sec)
         {
         FT8Mod1.cancelTransmit();
         FT8Mod1.sendData((char*) inputString[sendNext], 1, 0, 0);
         Serial.println("Send every 15 sec");
         doing15SecSend = true;
         }
   }      // End. loop

// Return true if time for new transmission
bool timing15(void)  {
   if(t60>=0.0f  && t60<1.0f)   return true;
   if(t60>=15.0f && t60<16.0f)  return true;
   if(t60>=30.0f && t60<31.0f)  return true;
   if(t60>=45.0f && t60<46.0f)  return true;
   return false;
   }
