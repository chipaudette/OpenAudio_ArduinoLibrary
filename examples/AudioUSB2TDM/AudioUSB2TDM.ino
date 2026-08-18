/*
 * USB<->TDM8 example for Teensy USB audio and OpenAudio_ArduinoLibrary
 *
 * Under Tools menu, set Audio sample rate (up to 96kHz), 8 channels, and desired USB bit depth.
 * Set USB type to Audio or Serial + MIDI + Audio
 * When making changes to USB settings, you may need to uninstall the device in Device Manager and reconnect for USB audio to work properly
 * 
 * Each TDM slot is 32 bits (8 slots x 32 bits = 256 bit clocks per frame).
 * Wiring to the Teensy 4.x SAI1 TDM pins:
 *   TX_DATA0 = pin 7, RX_DATA0 = pin 8
 *   MCLK = pin 23, BCLK = pin 21, SYNC = pin 20
 *
 * The codec/DSP on the other end must be configured for 8-slot TDM with
 * 32-bit slots, clocked from the Teensy (master) at 256 x fs bit clock.
 * Jump TX_DATA0 to RX_DATA0 to test loopback through USB out > TDM out > TDM in > USB in
 *
 * Note: TDM uses SAI1, the same peripheral as I2S, so I2S and TDM cannot be
 * used at the same time.
 */

#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>
#include "USB_Audio_F32.h"

// Both classes automatically expose the Tools > USB channels count.
// TDM8 uses SAI1 (pins 7/8/20/21/23) and drives the audio scheduler itself.
AudioInputUSB_F32      usb_in;
AudioOutputUSB_F32     usb_out;
AudioInputTDM8_F32     tdm_in;
AudioOutputTDM8_F32    tdm_out;


const int NUM_CHANNELS = AudioInputTDM8_F32::getNumChannels();
AudioConnection_F32   *in_patch[NUM_CHANNELS];
AudioConnection_F32   *out_patch[NUM_CHANNELS];

void setup() {
  for (int i=0; i < NUM_CHANNELS; i++){
    in_patch[i] = new AudioConnection_F32(usb_in, i, tdm_out, i);
    out_patch[i] = new AudioConnection_F32(tdm_in, i, usb_out, i);
  }
  
  AudioMemory(100);
  AudioMemory_F32(100);
}

void loop() {
}
