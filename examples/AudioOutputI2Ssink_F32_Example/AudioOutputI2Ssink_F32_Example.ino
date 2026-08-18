/*
 * Demonstration of AudioOutputI2Ssink_F32: an I2S output that does NOT
 * generate the bit clock (BCLK) or frame sync (FS).
 *
 * The audio data appears on pin 7 (SAI1_TX_DATA0).  Pins 21 (BCLK) and
 * 20 (FS) must be driven by an external clock SOURCE - a second Teensy
 * running a normal I2S output (e.g. AudioOutputI2S_F32_Example), a codec
 * in its clock-source role, an FPGA, etc.  This Teensy only listens to
 * the clocks and shifts out its data.  Connect GND between the devices.
 *
 * SLOT WORD WIDTH AUTO-DETECTION (default constructor):
 *   The sink measures the external clock's BCLK/FS ratio at startup and
 *   picks 16-, 24-, or 32-bit slots automatically.  A master-mode Teensy
 *   producing short frames (AudioOutputI2S_F32(16) -> 32 x fs BCLK or
 *   AudioOutputI2S_F32(24) -> 48 x fs BCLK) is detected as 16- or 24-bit;
 *   the default 64 x fs BCLK (2 slots x 32 bits) is detected as 32-bit.
 *
 * IMPORTANT: the measurement runs inside the constructor, before setup().
 * The external clock source must ALREADY be generating BCLK/FS when this
 * Teensy boots.  Power the clock-source first, let it start, then
 * power or reset the sink.  If no clock is present during the ~25 ms probe,
 * the sink defaults to 32-bit slots.  To force a width instead of measuring,
 * pass it to the constructor, e.g. AudioOutputI2Ssink_F32 i2s_out(24).
 *
 * Wiring:
 *   Data out  : pin 7
 *   BCLK in   : pin 21  (external clock source)
 *   FS in     : pin 20  (external clock source)
 *   MCLK      : pin 23  (not used in sink mode)
 *
 * The sink's DMA interrupt drives the audio block scheduler, so the audio
 * is clocked by the external clock source.
 *
 * Expected result: two different sine tones are present on pin 7 whenever the
 * external clock source is running.  The left channel is 1 kHz and the right
 * channel is 2 kHz
 */

#include <Arduino.h>
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>
#include <AudioStream_F32.h>

AudioSynthWaveformSine_F32 tone0;  // left channel (1 kHz)
AudioSynthWaveformSine_F32 tone1;  // right channel (2 kHz)
AudioOutputI2Ssink_F32 i2s_out;   // default constructor: auto-detect slot word width

AudioConnection_F32 connect0(tone0, 0, i2s_out, 0);
AudioConnection_F32 connect1(tone1, 0, i2s_out, 1);

void setup() {
  AudioMemory_F32(10);

  tone0.amplitude(0.5);
  tone0.frequency(1000.0);  // left: 1 kHz
  tone0.begin();
  tone1.amplitude(0.5);
  tone1.frequency(2000.0);  // right: 2 kHz
  tone1.begin();
}

void loop() {
  delay(1000);
}
