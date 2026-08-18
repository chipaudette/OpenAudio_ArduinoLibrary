# TDM8 / TDM16 Audio for Teensy 4.x

This extension adds TDM (time-division multiplexed) audio I/O to OpenAudio_ArduinoLibrary as Float32 modules. TDM is the standard way to move many channels over a single SAI data line: each frame carries N slots, and the frame sync line (SYNC) marks the start of each frame.

Two module sets are provided, each as separate classes (no channel-count constructor):

| Class | Channels | Slots per frame | Bit clock |
|---|---|---|---|
| `AudioInputTDM8_F32` / `AudioOutputTDM8_F32` | 8 | 8 × 32-bit | 256 × fs |
| `AudioInputTDM16_F32` / `AudioOutputTDM16_F32` | 16 | 16 × 32-bit | 512 × fs |

All slots are **32 bits**. Float samples in `[-1, +1]` are converted to/from `int32_t`:
- Input: `int32` slot value × `1/(2^31−1)` → float.
- Output: float, clamped to ±1, × `(2^31−1)` → `int32` slot value.

24-bit left-justified codecs take the top 24 bits of each 32-bit slot automatically, so both 24-bit and 32-bit slot codecs work without configuration.

## Wiring (Teensy 4.0 / 4.1, SAI1)

| Signal | Pin |
|---|---|
| TX_DATA0 | 7 |
| RX_DATA0 | 8 |
| MCLK | 23 |
| BCLK | 21 |
| SYNC | 20 |

The Teensy is the clock master. The codec/DSP on the other end must be configured for 8- or 16-slot TDM with 32-bit slots, clocked at 256 × fs (TDM8) or 512 × fs (TDM16).

> **Important:** TDM uses **SAI1**, the same peripheral as the I2S modules. You cannot use TDM and I2S at the same time on a Teensy 4.0/4.1. (The Teensy Audio library's TDM2 modules use SAI2, which is only available on the Teensy 4.1.)

## Usage

```cpp
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>

AudioInputTDM16_F32    tdm_in;
AudioOutputTDM16_F32   tdm_out;

constexpr int TDM_CHANNELS = AudioInputTDM16_F32::getNumChannels();
AudioConnection_F32   *patches[TDM_CHANNELS];

void setup() {
  for (int i = 0; i < TDM_CHANNELS; i++) {
    patches[i] = new AudioConnection_F32(tdm_in, i, tdm_out, i);
  }
  AudioMemory(16);
  AudioMemory_F32(100);
}

void loop() {
}
```

- `getNumChannels()` returns 8 or 16 depending on the class, so sketches can auto-create connections in a loop.
- Both classes support `AudioSettings_F32` (sample rate / block size) via the settings constructor.
- The TDM DMA ISR drives the audio scheduler (`update_responsibility`), so a standalone TDM in→TDM out chain runs without needing an I2S output.
- The SAI DMA uses a **ping-pong (double-buffered) half-buffer scheme** (`DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR`): the ISR always fills/reads the half the DMA is not currently transmitting/receiving, so the DMA never races a half-buffer being rewritten. This is the same structure as the Teensy core `output_tdm.cpp` / `output_i2s.cpp` and the F32 `output_i2s_f32.cpp` modules, and keeps the data path stable at high bit-clock rates (e.g. 24.576 MHz BCLK at 96 kHz).
- Only one instance of each TDM class should be used; the pins and SAI1 registers are shared. Creating both a TDM8 and TDM16 object in one sketch is not supported.

## Examples

- `AudioUSB2TDM` — USB ⇄ TDM8 8-channel passthrough, demonstrating the TDM8 F32 modules in a USB Audio 2.0 project. See `docs/MULTICHANNEL_USB_AUDIO.md` for USB setup.
