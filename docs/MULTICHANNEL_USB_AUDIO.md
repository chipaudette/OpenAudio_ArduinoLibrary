# Multi-Channel USB Audio for Teensy 4.x

This extension adds multi-channel USB Audio Class 2.0 support with asynchronous feedback to OpenAudio_ArduinoLibrary, enabling full-speed (12 Mbit) and high-speed (480 Mbit) multi-channel audio streaming between a Teensy 4.x and a host computer.

## Features

- **Multi-channel**: Up to 16 channels (2/4/6/8/10/12/14/16), configurable via `USB Audio Channels` in the Tools menu
- **Multiple sample rates**: 44.1, 48, 96, or 192kHz operation
- **Multiple bit depths**: 16, 24, or 32 bit integer formats
- **Asynchronous feedback**: USB audio output (Teensy → Host) uses an isochronous feedback endpoint so the host adapts its sample rate to the Teensy's actual clock, preventing buffer under/overruns.
- **PI controller**: A proportional-integral controller drives the feedback value, using a ring-buffer-based fill-rate estimator (`LastCall`) for stable rate tracking.
- **AudioClass 2.0**: Complies with the USB Audio 2.0 specification for wide host compatibility.
- **Float32 native**: `audio_block_f32_t` input and output from USB I/O objects, for direct compatibility with OpenAudio library.

## Compatibility

| OS | USB Audio 2.0 Support |
|---|---|
| Windows | Native for Windows 10 and later versions (UAC2 built-in) |
| macOS | Native (class-compliant) |
| Linux | Native (UAC2 driver) |
| ChromeOS | Native |

## Installation

### Prerequisites

1. Teensyduino 1.62.0 installed via Arduino Boards Manager or the Teensyduino installer
2. OpenAudio_ArduinoLibrary in your Arduino `libraries/` folder

### Quick Setup

From the OpenAudio library root:

```powershell
.\scripts\setup.ps1
```

The script handles everything:

1. **Detect Teensyduino** — Finds your install under `%LOCALAPPDATA%\Arduino15\`
2. **Backup** — Saves originals to `backups/<version>/`
3. **Patch core** — Installs multi-channel `usb_desc.h`, `usb_desc.c`, `usb.c`, `usb_audio.*`, `usb_audio_interface.*`
4. **Config** — Adds `boards.local.txt` (for USB Audio Channels menu) and updates `platform.txt`
5. **Arduino IDE library** — Copies the library into `Documents\Arduino\libraries\`
6. **Cache** — Clears Arduino IDE 2.x cache

After running, **restart Arduino IDE**. Examples appear under **File → Examples → OpenAudio_ArduinoLibrary**.

### Manual Steps (if the script doesn't work)

1. **Copy patched core files** from `patched_teensy_core/` to your Teensy core directory:
   ```
   %LOCALAPPDATA%\Arduino15\packages\teensy\hardware\avr\<version>\cores\teensy4\
   ```
2. **Copy config files** from `patched_teensy_core/config/`:
   - `boards.local.txt` → `%LOCALAPPDATA%\Arduino15\packages\teensy\hardware\avr\<version>\`
   - `BM-platform.txt` or `TD-platform.txt` → rename to `platform.txt` in same directory
3. **Link library** into Arduino's sketchbook libraries folder:
   ```
   mkdir "%USERPROFILE%\Documents\Arduino\libraries" -Force
   New-Item -ItemType Junction -Path "%USERPROFILE%\Documents\Arduino\libraries\OpenAudio_ArduinoLibrary" -Target "C:\path\to\OpenAudio_ArduinoLibrary"
   ```
4. **Restart Arduino IDE**

> **Note:** The library ships without a `library.properties`, matching upstream OpenAudio (which is installed by zip, not Library Manager).

## Usage

### Selecting USB Type

In Arduino IDE: **Tools → USB Type → "Audio"** or **"MIDI + Audio + Serial"**

If you installed `boards.local.txt`, under the **Tools** menu you will also see settings for **Audio block size, Audio sample rate, and USB Audio Channels**.

### Basic Sketch

```cpp
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>
#include "USB_Audio_F32.h"

// Both classes automatically expose the channel count configured in Tools > USB channels.
// Connect one AudioConnection_F32 per channel.
AudioInputUSB_F32    usb_in;
AudioOutputUSB_F32   usb_out;

// One connection per channel
AudioConnection_F32     patch0(usb_in, 0, usb_out, 0);
AudioConnection_F32     patch1(usb_in, 1, usb_out, 1);
// ... add more for channels 2-7 as needed

void setup() {
    AudioMemory(100);
    AudioMemory_F32(100);
}

void loop() {
    // Audio processing runs in the background
    delay(1);
}
```

Both classes automatically use the channel count configured in **Tools → USB channels** (the port arrays are sized at compile time). `AudioInputUSB_F32` transmits every received channel; `AudioOutputUSB_F32` accepts the same count of input ports. The compile-time constant `AudioInputUSB_F32::getNumChannels()` (or `AudioOutputUSB_F32::getNumChannels()`) returns that count, so sketches can auto-create loopback connections in a loop. See `USBAudioMultiChannel_F32` for a full 16-channel passthrough.

### Asynchronous vs Adaptive

By default, the output endpoint (Teensy → Host) runs in asynchronous mode. The Teensy sends a feedback value to the host, which adjusts its transmission rate accordingly. This eliminates the need for sample-rate conversion on the host side.

To switch to adaptive mode (simpler but may cause occasional glitches), comment out the `ASYNC_TX_ENDPOINT` define in `usb_desc.h`.

## Rates, Channels and Bit Depth Limits

USB bandwidth is finite. High speed (480 Mbit) can carry about **8192 bytes/ms**; full speed (12 Mbit) only **1023 bytes/ms**. The required bandwidth is roughly `sample_rate × channels × bytes_per_sample / 1000`.

Maximum bit depth that streams over **high speed**, per sample rate and channel count:

| Channels | 44.1kHz | 48kHz | 96kHz | 192kHz |
|---|:---:|:---:|:---:|:---:|
| 2 | 32-bit | 32-bit | 32-bit | 32-bit |
| 4 | 32-bit | 32-bit | 32-bit | 32-bit |
| 6 | 32-bit | 32-bit | 32-bit | 32-bit |
| 8 | 32-bit | 32-bit | 32-bit | 32-bit |
| 10 | 32-bit | 32-bit | 32-bit | 32-bit |
| 12 | 32-bit | 32-bit | 32-bit | 24-bit |
| 14 | 32-bit | 32-bit | 32-bit | 16-bit |
| 16 | 32-bit | 32-bit | 32-bit | 16-bit |

- If a combination exceeds the high-speed bandwidth, compilation fails with a missing `AUDIO_POLLING_INTERVAL_480` error — reduce channels or bit depth.
- On full speed, channels are automatically reduced to whatever fits (e.g. 16ch/48k/24-bit streams at 6ch over full speed). Use a high-speed link for the full channel count.
- At 176.4/192kHz with 24- or 32-bit samples, even 2 channels exceed full-speed bandwidth, so the full-speed descriptor falls back to 2 channels and streaming is only possible over a high-speed connection.
- When using more than 8 channels, the USB feature unit (volume/mute controls) is limited to the first 8 channels.


## Troubleshooting

### New menu options not appearing after setup
- After running the setup script, the Arduino IDE must fully load and finish "reloading boards" before new **Tools** menu options (e.g. **USB bit depth**) appear.
- On some systems the IDE takes **30+ seconds** to start up and reload boards. Let it finish loading — a notification such as "Reloading boards" will pop up in the corner — and only then check the **Tools** menu.
- If options still don't appear, close the IDE completely, re-run `.\scripts\update_teensy_audio.ps1` (which now replaces the menu block in `boards.txt` on every run), then restart the IDE and wait for it to fully load.

### Compilation errors about `USB_AUDIO_CHANNELS`
- The `boards.local.txt` may not be installed; the default falls back to 8 channels in `usb_desc.h`
- Check `usb_desc.h` for `USB_AUDIO_NO_CHANNELS_480` define

### Buffer underruns / glitches
- Increase `AudioMemory()` in your sketch
- Try adaptive mode (comment out `ASYNC_TX_ENDPOINT`)

### Restoring Original Files
```powershell
.\scripts\restore_teensy_audio.ps1
```

### Library examples not appearing in Arduino IDE

Arduino IDE looks for libraries under **File → Preferences → Sketchbook location** → `libraries/`. On Windows, OneDrive sometimes redirects `%USERPROFILE%\Documents` to a OneDrive path, causing the IDE to scan `C:\Users\<user>\OneDrive\Documents\Arduino\libraries\` instead of `C:\Users\<user>\Documents\Arduino\libraries\`.

**Fix:** In Arduino IDE, go to **File → Preferences** and check the **Sketchbook location** field. If it points to a OneDrive path, change it to `C:\Users\<user>\Documents\Arduino`.

Alternatively, copy or move `OpenAudio_ArduinoLibrary` into the OneDrive `libraries/` folder that the IDE is already scanning.

