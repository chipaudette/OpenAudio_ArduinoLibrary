/*
 * MIT License
 *
 * Copyright (c) 2025 alex6679
 * https://github.com/alex6679/teensy-4-usbAudio
 *
 * This file is derived from the teensy-4-usbAudio project (Teensy 4.x
 * multi-channel USB Audio 2.0 with asynchronous feedback).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef output_usb_f32_h_
#define output_usb_f32_h_

#include <Arduino.h>
#include "AudioStream_F32.h"
#include <usb_audio_interface.h>

#ifdef AUDIO_INTERFACE

class AudioOutputUSB_F32 : public AudioStream_F32
{
public:
	AudioOutputUSB_F32(void);
	AudioOutputUSB_F32(const AudioSettings_F32 &settings);
	virtual void update(void);
	void begin(void);
	static constexpr int getNumChannels() { return USB_AUDIO_MAX_NO_CHANNELS; }
	float getBufferedSamples() const;
	float getBufferedSamplesSmooth() const;
	float getActualBIntervalUs() const;
	USBAudioOutInterface::Status getStatus() const;

private:
	static void copy_from_buffers(uint8_t *dst, uint16_t bIdx, uint16_t noChannels, unsigned int count, unsigned int len);
	static void releaseBlocks(uint16_t bIdx, uint16_t noChannels);
	static bool isBlockReady(uint16_t bIdx, uint16_t channel);

	static audio_block_f32_t *txBuffer_f32[USBAudioOutInterface::ringTxBufferSize][USB_AUDIO_MAX_NO_CHANNELS];
	audio_block_f32_t *inputQueueArray_f32[USB_AUDIO_MAX_NO_CHANNELS];
	USBAudioOutInterface _usbInterface;
};

#endif // AUDIO_INTERFACE
#endif // output_usb_f32_h_
