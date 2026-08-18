/*
 *      input_tdm16_f32.cpp
 *
 *  Audio Library for Teensy 3.X
 * Copyright (c) 2017, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/*
 *  Extended by OpenAudio_ArduinoLibrary, 2026
 *  Converted to F32.  TDM16: 16 channels, one per 32-bit slot.
 *  Each 32-bit slot is received as an int32 and scaled to float in [-1, +1]
 *  (dividing by 2^31 correctly normalizes both 24-bit left-justified and
 *  true 32-bit slot data).
 */

#include <Arduino.h>
#include "input_tdm16_f32.h"
#include "output_tdm16_f32.h"
#include <arm_math.h>

#if defined(__IMXRT1062__)

// 2x buffer for ping-pong: DMA writes one half while the ISR reads the other
DMAMEM __attribute__((aligned(32)))
static uint32_t tdm_rx_buffer[AUDIO_BLOCK_SAMPLES * 16 * 2]; // 16 channels, one 32-bit slot each
audio_block_f32_t * AudioInputTDM16_F32::block_incoming[16] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
bool AudioInputTDM16_F32::update_responsibility = false;
DMAChannel AudioInputTDM16_F32::dma(false);
float AudioInputTDM16_F32::sample_rate_Hz = AUDIO_SAMPLE_RATE;
int AudioInputTDM16_F32::audio_block_samples = AUDIO_BLOCK_SAMPLES;

#define I32_TO_F32_NORM_FACTOR (4.656612875245797e-10)   //which is 1/(2^31 - 1)

void AudioInputTDM16_F32::begin(void)
{
	dma.begin(true); // Allocate the DMA channel first

	AudioOutputTDM16_F32::config_tdm();

	CORE_PIN8_CONFIG = 3;  //1:RX_DATA0
	IOMUXC_SAI1_RX_DATA0_SELECT_INPUT = 2;

	dma.TCD->SADDR = &I2S1_RDR0;
	dma.TCD->SOFF = 0;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
	dma.TCD->NBYTES_MLNO = 4;
	dma.TCD->SLAST = 0;
	dma.TCD->DADDR = tdm_rx_buffer;
	dma.TCD->DOFF = 4;
	dma.TCD->CITER_ELINKNO = audio_block_samples * 16 * 2;
	dma.TCD->DLASTSGA = -((int32_t)(audio_block_samples * 16 * 2 * 4));
	dma.TCD->BITER_ELINKNO = audio_block_samples * 16 * 2;
	// one interrupt per half buffer = one full block per channel per interrupt.
	// The ISR always reads the half the DMA is NOT currently filling, so it
	// never deinterleaves a half while the DMA is writing to it.
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_RX);

	update_responsibility = update_setup();
	dma.enable();

	// Note: do NOT touch I2S1_TCSR here.  Enabling TE before the output module
	// attaches its TX DMA makes the empty TX FIFO underrun and latch FEF, which
	// blocks the TX DMA requests when the output module enables it afterwards.
	// (Matches the proven IMXRT input_i2s pattern: RX only enables RCSR.)
	I2S1_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
	dma.attachInterrupt(isr);
}

void AudioInputTDM16_F32::isr(void)
{
	const int32_t *src;
	unsigned int ch;
	uint32_t daddr;

	daddr = (uint32_t)(dma.TCD->DADDR);
	dma.clearInterrupt();

	if (daddr < (uint32_t)tdm_rx_buffer + sizeof(tdm_rx_buffer) / 2) {
		// DMA is receiving to the first half, so read the second half
		src = (const int32_t *)(tdm_rx_buffer + audio_block_samples * 16);
	} else {
		// DMA is receiving to the second half, so read the first half
		src = (const int32_t *)tdm_rx_buffer;
	}

	#if IMXRT_CACHE_ENABLED >= 1
	arm_dcache_delete((void *)src, sizeof(tdm_rx_buffer) / 2);
	#endif

	if (block_incoming[0] != NULL) {
		// deinterleave: channel ch lives in slot ch of every 16-slot frame
		for (ch = 0; ch < 16; ch++) {
			float *dest = block_incoming[ch]->data;
			const int32_t *s = src + ch;
			for (int i = 0; i < audio_block_samples; i++) {
				*dest++ = (float32_t)*s * I32_TO_F32_NORM_FACTOR;
				s += 16;
			}
		}
	}
	if (update_responsibility) AudioStream_F32::update_all();
}

void AudioInputTDM16_F32::update(void)
{
	unsigned int i, j;
	audio_block_f32_t *new_block[16];
	audio_block_f32_t *out_block[16];

	// allocate 16 new blocks.  If any fails, allocate none
	for (i = 0; i < 16; i++) {
		new_block[i] = AudioStream_F32::allocate_f32();
		if (new_block[i] == NULL) {
			for (j = 0; j < i; j++) {
				AudioStream_F32::release(new_block[j]);
			}
			memset(new_block, 0, sizeof(new_block));
			break;
		}
	}
	__disable_irq();
	memcpy(out_block, block_incoming, sizeof(out_block));
	memcpy(block_incoming, new_block, sizeof(block_incoming));
	__enable_irq();
	if (out_block[0] != NULL) {
		// if we got 1 block, all 16 are filled
		for (i = 0; i < 16; i++) {
			transmit(out_block[i], i);
			AudioStream_F32::release(out_block[i]);
		}
	}
}

#endif
