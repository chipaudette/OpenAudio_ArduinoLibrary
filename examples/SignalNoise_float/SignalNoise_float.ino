/* SignalNoise_float.ino    Bob Larkin   19 June 2020
 *
 * Generate White Noise, Pink Noise, Gaussian White Noise and a Sine Wave
 * Combine all four in a adding "mixer", send to Codec and to peak/rms.
 *
 * Following is for all objects enabled (amplitudes non-zero)
 *   T3.6 Processor load, measured: 10.6%
 *   T4.0 Processor load, measured:  3.2%
 */

#include "Audio.h"
#include "OpenAudio_ArduinoLibrary.h"

// To work with T4.0 the I2S routine outputs 16-bit integer (I16).  Then
// use Audette I16 to F32 convert.  Same below for output, in reverse.
AudioInputI2S in1;
AudioSynthNoisePink_F32    pink1;
AudioSynthNoiseWhite_F32   white1;
AudioSynthGaussian_F32     gaussian1;
AudioSynthWaveformSine_F32 sine1;
AudioMixer4_F32            sum1;
AudioConvert_F32toI16      cnvrt1;     // Left
AudioConvert_F32toI16      cnvrt2;     // Right
AudioOutputI2S             i2sOut;
AudioAnalyzePeak_F32       peak1;
AudioAnalyzeRMS_F32        rms1;
AudioControlSGTL5000       codec;
AudioConnection_F32      connect1(pink1,     0, sum1,   0);
AudioConnection_F32      connect2(white1,    0, sum1,   1);
AudioConnection_F32      connect3(gaussian1, 0, sum1,   2);
AudioConnection_F32      connect4(sine1,     0, sum1,   3);
AudioConnection_F32      connect6(sum1,      0, cnvrt1, 0);   // Out to the CODEC left
AudioConnection_F32      connect7(sum1,      0, cnvrt2, 0);   // and right
AudioConnection_F32      connect8(sum1,      0, peak1,  0);
AudioConnection_F32      connect9(sum1,      0, rms1,   0);
AudioConnection          conI16_2(cnvrt1,    0, i2sOut, 0);   // DAC L
AudioConnection          conI16_3(cnvrt2,    0, i2sOut, 1);   // DAC R


// *********  Mini Control Panel  *********
//  Off/On switches, 0 for off, 1 for on
#define WHITE    0
#define PINK     0
#define GAUSSIAN 1
#define SINE     1

int gainControlDB = -35;  // Set gain in dB.
// *****************************************

void setup(void) {
  float32_t gain;
  AudioMemory(10);
  AudioMemory_F32(10);

  for (int i=0; i<4; i++) sum1.gain(i, 0.0);  // All off

  Serial.begin(1);  delay(1000);

  gain = powf( 10.0f, (gainControlDB/20.0f) );
  white1.amplitude(0.5f);
  gaussian1.amplitude(0.5f);
  sine1.frequency(1000.0f);
  sine1.amplitude(0.2f);
  codec.enable();
  delay(10);
  if(PINK)     sum1.gain(0, gain);
  else         sum1.gain(0, 0.0f);

  if(WHITE)    sum1.gain(1, gain);
  else         sum1.gain(1, 0.0f);

  if(GAUSSIAN) sum1.gain(2, gain);
  else         sum1.gain(2, 0.0f);

  if(SINE)     sum1.gain(3, gain);
  else         sum1.gain(3, 0.0f);
}

void loop(void) {
// Here is where the adjustment of the volume control could go.
// And anything else that needs regular attention, other
// than the audio stream.

  if (rms1.available() )  {Serial.print("RMS ="); Serial.println(rms1.read(), 6);}
  if (peak1.available() ) {Serial.print("P-P ="); Serial.println(peak1.readPeakToPeak(), 6);}
  Serial.print("CPU: Percent Usage, Max: ");
  Serial.print(AudioProcessorUsage());
  Serial.print(", ");
  Serial.print(AudioProcessorUsageMax());
  Serial.print("    ");
  Serial.print("Int16 Memory: ");
  Serial.print(AudioMemoryUsage());
  Serial.print(", ");
  Serial.print(AudioMemoryUsageMax());
  Serial.print("    ");
  Serial.print("Float Memory: ");
  Serial.print(AudioMemoryUsage_F32());
  Serial.print(", ");
  Serial.println(AudioMemoryUsageMax_F32());
  Serial.println();

  delay(1000);
}
