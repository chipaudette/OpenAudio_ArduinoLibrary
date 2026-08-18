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

#include "output_usb_f32.h"

#ifdef AUDIO_INTERFACE

// debug counters
volatile uint32_t usb_output_update_count = 0;
volatile uint32_t usb_output_fill_count = 0;

audio_block_f32_t *AudioOutputUSB_F32::txBuffer_f32[USBAudioOutInterface::ringTxBufferSize][USB_AUDIO_MAX_NO_CHANNELS];

AudioOutputUSB_F32::AudioOutputUSB_F32(void)
	: AudioStream_F32(USB_AUDIO_MAX_NO_CHANNELS, inputQueueArray_f32),
	  _usbInterface(releaseBlocks, isBlockReady, copy_from_buffers)
{
	begin();
	_usbInterface.begin();
}

AudioOutputUSB_F32::AudioOutputUSB_F32(const AudioSettings_F32 &settings)
	: AudioStream_F32(USB_AUDIO_MAX_NO_CHANNELS, inputQueueArray_f32),
	  _usbInterface(releaseBlocks, isBlockReady, copy_from_buffers)
{
	begin();
	_usbInterface.begin();
}

void AudioOutputUSB_F32::begin(void)
{
	for (uint16_t i = 0; i < USBAudioOutInterface::ringTxBufferSize; i++) {
		for (uint16_t j = 0; j < USB_AUDIO_MAX_NO_CHANNELS; j++) {
			txBuffer_f32[i][j] = NULL;
		}
	}
}

void AudioOutputUSB_F32::update(void)
{
	int16_t bIdx = -1;
	uint16_t noChannels;
	usb_output_update_count++;
	_usbInterface.update(bIdx, noChannels);
	if (bIdx < 0) {
		for (uint16_t i = 0; i < USB_AUDIO_MAX_NO_CHANNELS; i++) {
			audio_block_f32_t *b = receiveReadOnly_f32(i);
			if (b) {
				AudioStream_F32::release(b);
			}
		}
	}
	usb_output_fill_count++;
	for (uint16_t i = 0; i < noChannels; i++) {
		if (txBuffer_f32[bIdx][i]) {
			AudioStream_F32::release(txBuffer_f32[bIdx][i]);
		}
		txBuffer_f32[bIdx][i] = receiveReadOnly_f32(i);
		if (!txBuffer_f32[bIdx][i]) {
			if (!txBuffer_f32[bIdx][i]) {
				txBuffer_f32[bIdx][i] = AudioStream_F32::allocate_f32();
			}
			if (txBuffer_f32[bIdx][i]) {
				memset(txBuffer_f32[bIdx][i]->data, 0, AUDIO_BLOCK_SAMPLES * sizeof(float));
			} else {
				releaseBlocks(bIdx, noChannels);
				break;
			}
		}
	}
	_usbInterface.incrementBufferIndex();
}

float AudioOutputUSB_F32::getBufferedSamples() const
{
	return _usbInterface.getBufferedSamples();
}

float AudioOutputUSB_F32::getBufferedSamplesSmooth() const
{
	return _usbInterface.getBufferedSamplesSmooth();
}

float AudioOutputUSB_F32::getActualBIntervalUs() const
{
	return _usbInterface.getActualBIntervalUs();
}

USBAudioOutInterface::Status AudioOutputUSB_F32::getStatus() const
{
	return _usbInterface.getStatus();
}

#if AUDIO_SUBSLOT_SIZE == 2
void AudioOutputUSB_F32::copy_from_buffers(uint8_t *dst, uint16_t bIdx, uint16_t noChannels, unsigned int count, unsigned int len)
{
	int16_t *dst16 = (int16_t *)dst;
	for (uint32_t i = 0; i < len; i++) {
		for (uint16_t j = 0; j < noChannels; j++) {
			float sample = txBuffer_f32[bIdx][j]->data[count + i];
			if (sample > 1.0f) sample = 1.0f;
			else if (sample < -1.0f) sample = -1.0f;
			*dst16++ = (int16_t)(sample * 32767.0f);
		}
	}
}
#elif AUDIO_SUBSLOT_SIZE == 3
void AudioOutputUSB_F32::copy_from_buffers(uint8_t *dst, uint16_t bIdx, uint16_t noChannels, unsigned int count, unsigned int len)
{
	for (uint32_t i = 0; i < len; i++) {
		for (uint16_t j = 0; j < noChannels; j++) {
			float sample = txBuffer_f32[bIdx][j]->data[count + i];
			if (sample > 1.0f) sample = 1.0f;
			else if (sample < -1.0f) sample = -1.0f;
			int32_t val = (int32_t)(sample * 8388607.0f);
			*dst++ = val & 0xFF;
			*dst++ = (val >> 8) & 0xFF;
			*dst++ = (val >> 16) & 0xFF;
		}
	}
}
#elif AUDIO_SUBSLOT_SIZE == 4
void AudioOutputUSB_F32::copy_from_buffers(uint8_t *dst, uint16_t bIdx, uint16_t noChannels, unsigned int count, unsigned int len)
{
	int32_t *dst32 = (int32_t *)dst;
	for (uint32_t i = 0; i < len; i++) {
		for (uint16_t j = 0; j < noChannels; j++) {
			float sample = txBuffer_f32[bIdx][j]->data[count + i];
			if (sample > 1.0f) sample = 1.0f;
			else if (sample < -1.0f) sample = -1.0f;
			*dst32++ = (int32_t)(sample * 2147483647.0f);
		}
	}
}
#else
#error "Unsupported AUDIO_SUBSLOT_SIZE (supported: 2, 3, 4)"
#endif

void AudioOutputUSB_F32::releaseBlocks(uint16_t bIdx, uint16_t noChannels)
{
	for (uint16_t i = 0; i < noChannels; i++) {
		if (txBuffer_f32[bIdx][i]) {
			AudioStream_F32::release(txBuffer_f32[bIdx][i]);
			txBuffer_f32[bIdx][i] = NULL;
		}
	}
}

bool AudioOutputUSB_F32::isBlockReady(uint16_t bIdx, uint16_t channel)
{
	return txBuffer_f32[bIdx][channel] != NULL;
}

#endif
