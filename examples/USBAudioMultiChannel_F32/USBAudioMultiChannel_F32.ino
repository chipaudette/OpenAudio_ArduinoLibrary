#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>
#include "USB_Audio_F32.h"

// Set audio parameters using Tools menu
// USB type must be Audio or Serial + MIDI + Audio
// Number of channels
// Sample rate
// Bit depth

// Both classes automatically expose the Tools > USB channels count.
AudioInputUSB_F32      usb_in;
AudioOutputUSB_F32     usb_out;

// USB->USB requires an audio output ISR to drive the scheduler.
#include "output_i2s_f32.h"
#include "input_i2s_f32.h"
AudioOutputI2S_F32       i2s_out;
AudioInputI2S_F32       i2s_in;
AudioConnection_F32      s0(usb_in, 0, i2s_out, 0);
AudioConnection_F32      s1(usb_in, 1, usb_out, 1);
AudioConnection_F32      s2(i2s_in, 0, usb_out, 0);

void setup() {
  AudioMemory(100);
  AudioMemory_F32(100);
}

void loop() {
}
