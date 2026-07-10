/*
 *      input_i2s_quad_f32.cpp
 *
 *  Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
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
 *  Extended by Terrance Robertson, May 2025
 *  Converted to F32
 */

#include <Arduino.h>
#include "input_i2s_quad_f32.h"
#include "output_i2s_quad_f32.h"
#include "output_i2s_f32.h"

DMAMEM __attribute__((aligned(32))) static uint32_t i2s_rx_buffer[AUDIO_BLOCK_SAMPLES*2];
audio_block_f32_t * AudioInputI2SQuad_F32::block_ch1 = NULL;
audio_block_f32_t * AudioInputI2SQuad_F32::block_ch2 = NULL;
audio_block_f32_t * AudioInputI2SQuad_F32::block_ch3 = NULL;
audio_block_f32_t * AudioInputI2SQuad_F32::block_ch4 = NULL;
uint16_t AudioInputI2SQuad_F32::block_offset = 0;
bool AudioInputI2SQuad_F32::update_responsibility = false;
DMAChannel AudioInputI2SQuad_F32::dma(false);

float AudioInputI2SQuad_F32::sample_rate_Hz = AUDIO_SAMPLE_RATE;
int AudioInputI2SQuad_F32::audio_block_samples = AUDIO_BLOCK_SAMPLES;

void AudioInputI2SQuad_F32::begin(void) {
	dma.begin(true); // Allocate the DMA channel first

	AudioOutputI2S_F32::config_i2s();
  CORE_PIN8_CONFIG = 3;
  CORE_PIN6_CONFIG = 3;
  IOMUXC_SAI1_RX_DATA0_SELECT_INPUT = 2; // GPIO_B1_00_ALT3, pg 873
  IOMUXC_SAI1_RX_DATA1_SELECT_INPUT = 1; // GPIO_B0_10_ALT3, pg 873
	dma.TCD->SADDR = (void *)((uint32_t)&I2S1_RDR0 + 2);
	dma.TCD->SOFF = 4;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
	dma.TCD->NBYTES_MLOFFYES = DMA_TCD_NBYTES_SMLOE |
		DMA_TCD_NBYTES_MLOFFYES_MLOFF(-8) |
		DMA_TCD_NBYTES_MLOFFYES_NBYTES(4);
	dma.TCD->SLAST = -8;
	dma.TCD->DADDR = i2s_rx_buffer;
	dma.TCD->DOFF = 2;
	dma.TCD->CITER_ELINKNO = AUDIO_BLOCK_SAMPLES * 2;
	dma.TCD->DLASTSGA = -sizeof(i2s_rx_buffer);
	dma.TCD->BITER_ELINKNO = AUDIO_BLOCK_SAMPLES * 2;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_RX);

	I2S1_RCSR = 0;
	I2S1_RCR3 = I2S_RCR3_RCE_2CH;

	I2S1_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
	update_responsibility = update_setup();
	dma.enable();
	dma.attachInterrupt(isr);
}

void AudioInputI2SQuad_F32::isr(void) {
	uint32_t daddr, offset;
	const int16_t *src;
	float32_t *dest1, *dest2, *dest3, *dest4;

	daddr = (uint32_t)(dma.TCD->DADDR);
	dma.clearInterrupt();

	if(daddr < (uint32_t)i2s_rx_buffer + sizeof(i2s_rx_buffer) / 2) {
		// DMA is receiving to the first half of the buffer
		// need to remove data from the second half
		src = (int16_t *)&i2s_rx_buffer[AUDIO_BLOCK_SAMPLES];
		if(update_responsibility) update_all();
	} else {
		// DMA is receiving to the second half of the buffer
		// need to remove data from the first half
		src = (int16_t *)&i2s_rx_buffer[0];
	}
	if(block_ch1) {
		offset = block_offset;
		if(offset <= AUDIO_BLOCK_SAMPLES/2) {
			arm_dcache_delete((void*)src, sizeof(i2s_rx_buffer) / 2);
			block_offset = offset + AUDIO_BLOCK_SAMPLES/2;
			dest1 = &(block_ch1->data[offset]);
			dest2 = &(block_ch2->data[offset]);
			dest3 = &(block_ch3->data[offset]);
			dest4 = &(block_ch4->data[offset]);
			for (int i=0; i < AUDIO_BLOCK_SAMPLES/2; i++) {
				*dest1++ = (float32_t)*src++;
				*dest3++ = (float32_t)*src++;
				*dest2++ = (float32_t)*src++;
				*dest4++ = (float32_t)*src++;
			}
		}
	}
}

#define I16_TO_F32_NORM_FACTOR (3.051850947599719e-05)  //which is 1/32767
void AudioInputI2SQuad_F32::scale_i16_to_f32( float32_t *p_i16, float32_t *p_f32, int len) {
	for (int i=0; i<len; i++) { *p_f32++ = ((*p_i16++) * I16_TO_F32_NORM_FACTOR); }
}
#define I24_TO_F32_NORM_FACTOR (1.192093037616377e-07)   //which is 1/(2^23 - 1)
void AudioInputI2SQuad_F32::scale_i24_to_f32( float32_t *p_i24, float32_t *p_f32, int len) {
	for (int i=0; i<len; i++) { *p_f32++ = ((*p_i24++) * I24_TO_F32_NORM_FACTOR); }
}
#define I32_TO_F32_NORM_FACTOR (4.656612875245797e-10)   //which is 1/(2^31 - 1)
void AudioInputI2SQuad_F32::scale_i32_to_f32( float32_t *p_i32, float32_t *p_f32, int len) {
	for (int i=0; i<len; i++) { *p_f32++ = ((*p_i32++) * I32_TO_F32_NORM_FACTOR); }
}

void AudioInputI2SQuad_F32::update(void) {
	audio_block_f32_t *new1, *new2, *new3, *new4;
	audio_block_f32_t *out1, *out2, *out3, *out4;

	// allocate 4 new blocks
	new1 = AudioStream_F32::allocate_f32();
	new2 = AudioStream_F32::allocate_f32();
	new3 = AudioStream_F32::allocate_f32();
	new4 = AudioStream_F32::allocate_f32();
	// but if any fails, allocate none
	if(!new1 || !new2 || !new3 || !new4) {
		if(new1) {
			AudioStream_F32::release(new1);
			new1 = NULL;
		}
		if(new2) {
			AudioStream_F32::release(new2);
			new2 = NULL;
		}
		if(new3) {
			AudioStream_F32::release(new3);
			new3 = NULL;
		}
		if(new4) {
			AudioStream_F32::release(new4);
			new4 = NULL;
		}
	}
	__disable_irq();
	if(block_offset >= AUDIO_BLOCK_SAMPLES) {
		// the DMA filled 4 blocks, so grab them and get the
		// 4 new blocks to the DMA, as quickly as possible
		out1 = block_ch1;
		block_ch1 = new1;
		out2 = block_ch2;
		block_ch2 = new2;
		out3 = block_ch3;
		block_ch3 = new3;
		out4 = block_ch4;
		block_ch4 = new4;
		block_offset = 0;
		__enable_irq();

	  //scale the float values so that the maximum possible audio values span -1.0 to + 1.0
	  scale_i16_to_f32(out1->data, out1->data, AUDIO_BLOCK_SAMPLES);
	  scale_i16_to_f32(out2->data, out2->data, AUDIO_BLOCK_SAMPLES);
	  scale_i16_to_f32(out3->data, out3->data, AUDIO_BLOCK_SAMPLES);
	  scale_i16_to_f32(out4->data, out4->data, AUDIO_BLOCK_SAMPLES);

    // then transmit the DMA's former blocks
		AudioStream_F32::transmit(out1, 0);
		AudioStream_F32::release(out1);
		AudioStream_F32::transmit(out2, 1);
		AudioStream_F32::release(out2);
		AudioStream_F32::transmit(out3, 2);
		AudioStream_F32::release(out3);
		AudioStream_F32::transmit(out4, 3);
		AudioStream_F32::release(out4);
	} else if(new1 != NULL) {
		// the DMA didn't fill blocks, but we allocated blocks
		if(block_ch1 == NULL) {
			// the DMA doesn't have any blocks to fill, so
			// give it the ones we just allocated
			block_ch1 = new1;
			block_ch2 = new2;
			block_ch3 = new3;
			block_ch4 = new4;
			block_offset = 0;
			__enable_irq();
		} else {
			// the DMA already has blocks, doesn't need these
			__enable_irq();
			AudioStream_F32::release(new1);
			AudioStream_F32::release(new2);
			AudioStream_F32::release(new3);
			AudioStream_F32::release(new4);
		}
	} else {
		// The DMA didn't fill blocks, and we could not allocate
		// memory... the system is likely starving for memory!
		// Sadly, there's nothing we can do.
		__enable_irq();
	}
}
