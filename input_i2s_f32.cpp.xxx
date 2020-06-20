/* Audio Library for Teensy 3.X
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

#include "input_i2s_f32.h"
#include "output_i2s_f32.h"
#include <arm_math.h>

DMAMEM static uint32_t i2s_rx_buffer[AUDIO_BLOCK_SAMPLES];
audio_block_t * AudioInputI2S_F32::block_left = NULL;
audio_block_t * AudioInputI2S_F32::block_right = NULL;
uint16_t AudioInputI2S_F32::block_offset = 0;
bool AudioInputI2S_F32::update_responsibility = false;
DMAChannel AudioInputI2S_F32::dma(false);


float AudioInputI2S_F32::sample_rate_Hz = AUDIO_SAMPLE_RATE;
int AudioInputI2S_F32::audio_block_samples = AUDIO_BLOCK_SAMPLES;

#define I2S_BUFFER_TO_USE_BYTES (AudioOutputI2S_F32::audio_block_samples*sizeof(i2s_rx_buffer[0]))

void AudioInputI2S_F32::begin(void)
{
	dma.begin(true); // Allocate the DMA channel first

	//block_left_1st = NULL;
	//block_right_1st = NULL;

	// TODO: should we set & clear the I2S_RCSR_SR bit here?
	AudioOutputI2S_F32::sample_rate_Hz = sample_rate_Hz;
	AudioOutputI2S_F32::audio_block_samples = audio_block_samples;
	AudioOutputI2S_F32::config_i2s();

	CORE_PIN13_CONFIG = PORT_PCR_MUX(4); // pin 13, PTC5, I2S0_RXD0
#if defined(KINETISK)
	dma.TCD->SADDR = &I2S0_RDR0;
	dma.TCD->SOFF = 0;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
	dma.TCD->NBYTES_MLNO = 2;
	dma.TCD->SLAST = 0;
	dma.TCD->DADDR = i2s_rx_buffer;
	dma.TCD->DOFF = 2;
	//dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;	//original
	dma.TCD->CITER_ELINKNO = I2S_BUFFER_TO_USE_BYTES / 2;
	//dma.TCD->DLASTSGA = -sizeof(i2s_rx_buffer); //original
	dma.TCD->DLASTSGA = -I2S_BUFFER_TO_USE_BYTES;
	//dma.TCD->BITER_ELINKNO = sizeof(i2s_rx_buffer) / 2;	//original
	dma.TCD->BITER_ELINKNO = I2S_BUFFER_TO_USE_BYTES / 2;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
#endif
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
	update_responsibility = update_setup();
	dma.enable();

	I2S0_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
	I2S0_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE; // TX clock enable, because sync'd to TX
	dma.attachInterrupt(isr);
	
	
};

void AudioInputI2S_F32::isr(void)
{
	uint32_t daddr, offset;
	const int16_t *src, *end;
	int16_t *dest_left, *dest_right;
	audio_block_t *left, *right;

	//digitalWriteFast(3, HIGH);
#if defined(KINETISK)
	daddr = (uint32_t)(dma.TCD->DADDR);
#endif
	dma.clearInterrupt();

	//if (daddr < (uint32_t)i2s_rx_buffer + sizeof(i2s_rx_buffer) / 2) {
	if (daddr < (uint32_t)i2s_rx_buffer + I2S_BUFFER_TO_USE_BYTES / 2) {		
		
		// DMA is receiving to the first half of the buffer
		// need to remove data from the second half
		//src = (int16_t *)&i2s_rx_buffer[AUDIO_BLOCK_SAMPLES/2];	//original
		//end = (int16_t *)&i2s_rx_buffer[AUDIO_BLOCK_SAMPLES];	//original
		src = (int16_t *)&i2s_rx_buffer[audio_block_samples/2];
		end = (int16_t *)&i2s_rx_buffer[audio_block_samples];		
		if (AudioInputI2S_F32::update_responsibility) AudioStream_F32::update_all();
	} else {
		// DMA is receiving to the second half of the buffer
		// need to remove data from the first half
		src = (int16_t *)&i2s_rx_buffer[0];
		//end = (int16_t *)&i2s_rx_buffer[AUDIO_BLOCK_SAMPLES/2];	//original
		end = (int16_t *)&i2s_rx_buffer[audio_block_samples/2];
	}
	left = AudioInputI2S_F32::block_left;
	right = AudioInputI2S_F32::block_right;
	if (left != NULL && right != NULL) {
		offset = AudioInputI2S_F32::block_offset;
		//if (offset <= AUDIO_BLOCK_SAMPLES/2) {	//original
		if (offset <= ((uint32_t) audio_block_samples/2)) {
			dest_left = &(left->data[offset]);
			dest_right = &(right->data[offset]);
			//AudioInputI2S_F32::block_offset = offset + AUDIO_BLOCK_SAMPLES/2;	//original
			AudioInputI2S_F32::block_offset = offset + audio_block_samples/2;
			do {
				//n = *src++;
				//*dest_left++ = (int16_t)n;
				//*dest_right++ = (int16_t)(n >> 16);
				*dest_left++ = *src++;
				*dest_right++ = *src++;
			} while (src < end);
		}
	}
	//digitalWriteFast(3, LOW);
}

#define I16_TO_F32_NORM_FACTOR (3.051757812500000E-05)  //which is 1/32768 
void AudioInputI2S_F32::convert_i16_to_f32( int16_t *p_i16, float32_t *p_f32, int len) {
	for (int i=0; i<len; i++) { *p_f32++ = ((float32_t)(*p_i16++)) * I16_TO_F32_NORM_FACTOR; }
}

void AudioInputI2S_F32::update(void)
{
	audio_block_t *new_left=NULL, *new_right=NULL, *out_left=NULL, *out_right=NULL;

	// allocate 2 new blocks, but if one fails, allocate neither
	new_left = AudioStream::allocate();
	if (new_left != NULL) {
		new_right = AudioStream::allocate();
		if (new_right == NULL) {
			AudioStream::release(new_left);
			new_left = NULL;
		}
	}
	__disable_irq();
	//if (block_offset >= AUDIO_BLOCK_SAMPLES) {  //original
	if (block_offset >= audio_block_samples) {	
		// the DMA filled 2 blocks, so grab them and get the
		// 2 new blocks to the DMA, as quickly as possible
		out_left = block_left;
		block_left = new_left;
		out_right = block_right;
		block_right = new_right;
		block_offset = 0;
		__enable_irq();
		
		// then transmit the DMA's former blocks
		
		//  but, first, convert them to F32
		audio_block_f32_t *out_left_f32=NULL, *out_right_f32=NULL;
		out_left_f32 = AudioStream_F32::allocate_f32();
		if (out_left_f32 != NULL) {
			out_right_f32 = AudioStream_F32::allocate_f32();
			if (out_right_f32 == NULL) {
				AudioStream_F32::release(out_left_f32);
				out_left_f32 = NULL;
			}
		}
		if (out_left_f32 != NULL) {
			//convert int16 to float 32
			convert_i16_to_f32(out_left->data, out_left_f32->data, audio_block_samples);
			convert_i16_to_f32(out_right->data, out_right_f32->data, audio_block_samples);
			
			//transmit the f32 data!
			AudioStream_F32::transmit(out_left_f32,0);
			AudioStream_F32::transmit(out_right_f32,1);
			AudioStream_F32::release(out_left_f32);
			AudioStream_F32::release(out_right_f32);
		}
		AudioStream::release(out_left);
		AudioStream::release(out_right);
		//Serial.print(".");
	} else if (new_left != NULL) {
		// the DMA didn't fill blocks, but we allocated blocks
		if (block_left == NULL) {
			// the DMA doesn't have any blocks to fill, so
			// give it the ones we just allocated
			block_left = new_left;
			block_right = new_right;
			block_offset = 0;
			__enable_irq();
		} else {
			// the DMA already has blocks, doesn't need these
			__enable_irq();
			AudioStream::release(new_left);
			AudioStream::release(new_right);
		}
	} else {
		// The DMA didn't fill blocks, and we could not allocate
		// memory... the system is likely starving for memory!
		// Sadly, there's nothing we can do.
		__enable_irq();
	}
}


/******************************************************************/

/*
void AudioInputI2Sslave::begin(void)
{
	dma.begin(true); // Allocate the DMA channel first

	//block_left_1st = NULL;
	//block_right_1st = NULL;

	AudioOutputI2Sslave::config_i2s();

	CORE_PIN13_CONFIG = PORT_PCR_MUX(4); // pin 13, PTC5, I2S0_RXD0
#if defined(KINETISK)
	dma.TCD->SADDR = &I2S0_RDR0;
	dma.TCD->SOFF = 0;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
	dma.TCD->NBYTES_MLNO = 2;
	dma.TCD->SLAST = 0;
	dma.TCD->DADDR = i2s_rx_buffer;
	dma.TCD->DOFF = 2;
	dma.TCD->CITER_ELINKNO = sizeof(i2s_rx_buffer) / 2;
	dma.TCD->DLASTSGA = -sizeof(i2s_rx_buffer);
	dma.TCD->BITER_ELINKNO = sizeof(i2s_rx_buffer) / 2;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
#endif
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
	update_responsibility = update_setup();
	dma.enable();

	I2S0_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
	I2S0_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE; // TX clock enable, because sync'd to TX
	dma.attachInterrupt(isr);
}
*/



