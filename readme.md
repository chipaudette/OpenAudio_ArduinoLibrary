OpenAudio Library for Teensy
===========================

**Purpose**: The purpose of this library is to build upon the [Teensy Audio Library](http://www.pjrc.com/teensy/td_libs_Audio.html) to enable new functionality for real-time audio processing.

**Approach**: I am attempting to follow the structure and approach of the Teensy Audio Library so that coding techniques, style, and structure that works for the Teensy Audio Library will also work with my library.  

**Send Help!**:  While I'm hoping to follow the pattern of the Teensy Audio Library, I am not a well trained programmer, so I have difficulty chosing the right path.  If you see ways to make my code better aligned with the Teensy Audio Library, send a pull request!

Contents
---------

As I said, the goal of this library is to extend the functionality of the Teensy Audio Library.  My extensions are focused in these areas:

**Floating Point Audio Processing**:  The Teensy Audio Library processes the audio data as fixed-point Int16 values.  While very fast for the Teensy 3.0-3.2, it is unnecessarily constraining (IMO) for the Teensy 3.5 and 3.6, which both have floating-point units.  To enable floating-point audio processing, I have created:
* **AudioStream_F32: Following the model of Teensy's `AudioStream` class.  In addition to the new `AudioStream_F32`, this file includes a new `audio_block_f32_t` in place of `audio_block_t`, as well as a new `AudioConnection_F32` in place of `AudioConnection`.
* **New F32 Audio Processing Blocks**: Using this new floating-point capability, I've written some new audio processing blocks that operate using floating-point audio values instead of fixed-point values.  Examples include `AudioEffectGain_F32` and `AudioEffectComperessor_F32`.
* **F32 Versions of Existing Blocks**: To maintain continuity with the Teensy Audio Library, a number of the Audio Library's Audio blocks (which process as Int16 data) to operate on floating-point (ie, Float32) data.  Thanks to other people's contributions, examples include `AudioMixer4_F32` and `AudioMultiply_F32`.  Feel free to convert more blocks, if you'd like!
* **New Hardware Controls**: The Teensy Audio Library does a great job of wrapping up the complicated control of hardware elements (such as the SGTL5000 audio codec) into a nice easy-to-use classes.  But, sometimes, certain features of the hardware were not exposed in those classes.  Here, I've extended the default classes to expose the extra features that I want.  See `AudioControlSGTL5000_Extended` as an example.

After installing this library into your Arduino->Libraries direction, you can have access to any of these capabilities simply by including the following command in your Arduino sketch: `#include <OpenAudio_ArduinoLibrary.h>`.

Installation
------------

Download (and unzip) this library into the Arduino libraries directory on your computer.  On my computer, that means I put it here:

`C:\Users\chipaudette\Documents\Arduino\libraries\OpenAudio_ArduinoLibrary`

Restart your Arduino IDE and you should now see this libraries example sketches under File->Examples->OpenAudio_ArduinoLibrary

Dependencies
------------

This library extends the functionality of the [Teensy Audio Library](http://www.pjrc.com/teensy/td_libs_Audio.html), so you'll need to install it per its instructions.

The floating-point processing takes advantage of the DSP acceleration afforded by the ARM M4F core inside the Teensy 3.5/3.6 processor.  Therefore, it uses `arm_math.h`.  This dependencies is installed automatically when you install the Teensy Audio Library, so you don't need to take any extra steps.  It's there already.

