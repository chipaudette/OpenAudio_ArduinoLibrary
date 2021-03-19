OpenAudio Library for Teensy
============================
This is a library of Teensy Arduino classes to provide audio signal processing with 32-bit floating point data transfers.
This parallels the 16-bit integer [Teensy Audio Library](http://www.pjrc.com/teensy/td_libs_Audio.html) but, interconnects blocks using 32-bit floating point data types.  Converters for inter-connecting the two types are provided.  Both data types can be used in the same sketch.  To distinguish the two types, file and class names for this library have a trailing "_F32."

This library was originally put together by Chip Audette and he summed up the reasons for this:

**Purpose**: The purpose of this library is to build upon the [Teensy Audio Library](http://www.pjrc.com/teensy/td_libs_Audio.html) to enable new functionality for real-time audio processing.

**Approach**: To follow the structure and approach of the Teensy Audio Library, so that coding techniques, style, and structure that works for the Teensy Audio Library will also work with this library.

**Send Help!**:  If you see ways to make my code better aligned with the Teensy Audio Library, send a pull request!

Information
-----------
**Design Tool** Many changes and additions were made in the 2020 time period.  Hopefully this has brought this library to the point where blocks can be just assembled and run.  To make that practical, there is now an [OpenAudio Library Design Tool](http://www.janbob.com/electron/OpenAudio_Design_Tool/index.html) that again parallels the Teensy Audio Design Tool, that has been borrowed upon for both code and content. This OpenAudio Design Tool is intended to be a temporary design aid with improvements to come, like being able to combine integer and floating point blocks.

The primary documentation is the Design Tool.  Clicking on any of the classes brings up a right-side help panel.  This lists the use of the class, the constraints, all the functions and at the botom are general notes.  Most classes have example INO files and these are listed and available when this library is installed.  Sometimes elements of this documentation is not  completed, yet.  In some of those cases it is worth looking at the .h include file for the class.  At the top of these files are comments with more information.

**Teensy 3 and 4** During 2020 this library has undergone revision to make it Teensy 4.x compatible.  Much of this related to using multiple sampling rates with the I2S clocks and the interrupt/DMA code for data transfer. The files and associated classes output_i2s_f32.h, output_i2s_f32.cpp, input_i2s_f32.h, input_i2s_f32.cpp are now
ready to be used for T3.x and T4.x.  There are some restrictions, particularly this should be used with 16-bit I2S codec data. Codec sample rates can be varied. Variable block size is supported, but be sure the settings option is used.  Thanks to Chip, @jcj83429 and all the Teensy development folks.

**Tympan Project** Many of the classes in this library were put together as part of the [Tympan Project.](https://github.com/Tympan)  That is oriented towards open-source hearing aid and hearing aid development tools. It has its own [Tympan Design Tool](https://tympan.github.io/Tympan_Audio_Design_Tool/) as well as some custom Teensy-based hardware. Additionally, there are a few classes in this library that use terminology and variables that are specific to audiology.  It is intended that these, in time, be replaced by similar classes with more conventional descriptors.  And, of course, if your interest is in hearing aids, you should spend time at the Tympan project!

Installation
------------

Download (and unzip) this library into the Arduino libraries directory on your computer.  On my computer, that means I put it here:

`C:\Users\chipaudette\Documents\Arduino\libraries\OpenAudio_ArduinoLibrary`

Restart your Arduino IDE and you should now see this libraries example sketches under File->Examples->OpenAudio_ArduinoLibrary

After installing this library into your Arduino->Libraries direction, you can have access to any of these capabilities simply by including the following command in your Arduino sketch: `#include <OpenAudio_ArduinoLibrary.h>`.

As an alternative to the ZIP download, you can use git to maintain a local copy of the library.  This has the advantage of easy updating.  See GitHub and git documentation on how to do this.

Dependencies
------------

This library extends the functionality of the [Teensy Audio Library](http://www.pjrc.com/teensy/td_libs_Audio.html), so you'll need to install it per its instructions.

The floating-point processing takes advantage of the DSP acceleration afforded by the ARM core inside the Teensy 3.x  and 4.x processor.  Therefore, it uses `arm_math.h`.  This dependencies is installed automatically when you install the Teensy Audio Library, so you don't need to take any extra steps.  It's there already.
