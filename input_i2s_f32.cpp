/*
 *      input_i2s_f32.cpp
 * 
 * Audio Library for Teensy 3.X
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
 *  Extended by Chip Audette, OpenAudio, May 2019
 *  Converted to F32 and to variable audio block length
 *	The F32 conversion is under the MIT License.  Use at your own risk.
 */
// Updated OpenAudio F32 with this version from Chip Audette's Tympan Library Jan 2021 RSL

#include <Arduino.h>  //do we really need this? (Chip: 2020-10-31)
#include "input_i2s_f32.h"
#include "output_i2s_f32.h"

#include <arm_math.h>

//DMAMEM __attribute__((aligned(32))) 
static uint32_t i2s_rx_buffer[AUDIO_BLOCK_SAMPLES]; //good for 16-bit audio samples coming in from teh AIC.  32-bit transfers will need this to be bigger.
audio_block_f32_t * AudioInputI2S_F32::block_left_f32 = NULL;
audio_block_f32_t * AudioInputI2S_F32::block_right_f32 = NULL;
uint16_t AudioInputI2S_F32::block_offset = 0;
bool AudioInputI2S_F32::update_responsibility = false;
DMAChannel AudioInputI2S_F32::dma(false);

int AudioInputI2S_F32::flag_out_of_memory = 0;
unsigned long AudioInputI2S_F32::update_counter = 0;

float AudioInputI2S_F32::sample_rate_Hz = AUDIO_SAMPLE_RATE;
int AudioInputI2S_F32::audio_block_samples = AUDIO_BLOCK_SAMPLES;

//#for 16-bit transfers
#define I2S_BUFFER_TO_USE_BYTES (AudioOutputI2S_F32::audio_block_samples*sizeof(i2s_rx_buffer[0]))

//#for 32-bit transfers
//#define I2S_BUFFER_TO_USE_BYTES (AudioOutputI2S_F32::audio_block_samples*2*sizeof(i2s_rx_buffer[0]))

void AudioInputI2S_F32::begin(void) {
	bool transferUsing32bit = false;
	begin(transferUsing32bit);
}

void AudioInputI2S_F32::begin(bool transferUsing32bit) {
	dma.begin(true); // Allocate the DMA channel first

	AudioOutputI2S_F32::sample_rate_Hz = sample_rate_Hz; //these were given in the AudioSettings in the contructor
	AudioOutputI2S_F32::audio_block_samples = audio_block_samples;//these were given in the AudioSettings in the contructor
	
	//block_left_1st = NULL;
	//block_right_1st = NULL;

	// TODO: should we set & clear the I2S_RCSR_SR bit here?
	AudioOutputI2S_F32::config_i2s(transferUsing32bit);

#if defined(KINETISK)
	CORE_PIN13_CONFIG = PORT_PCR_MUX(4); // pin 13, PTC5, I2S0_RXD0
	dma.TCD->SADDR = (void *)((uint32_t)&I2S0_RDR0 + 2);  //From Teensy Audio Library...but why "+ 2"  (Chip 2020-10-31)
	dma.TCD->SOFF = 0;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
	dma.TCD->NBYTES_MLNO = 2;
	dma.TCD->SLAST = 0;
	dma.TCD->DADDR = i2s_rx_buffer;
	dma.TCD->DOFF = 2;
	//dma.TCD->CITER_ELINKNO = sizeof(i2s_rx_buffer) / 2;  //original from Teensy Audio Library
	dma.TCD->CITER_ELINKNO = I2S_BUFFER_TO_USE_BYTES / 2;
	//dma.TCD->DLASTSGA = -sizeof(i2s_rx_buffer);  //original from Teensy Audio Library
	dma.TCD->DLASTSGA = -I2S_BUFFER_TO_USE_BYTES;
	//dma.TCD->BITER_ELINKNO = sizeof(i2s_rx_buffer) / 2;  //original from Teensy Audio Library
	dma.TCD->BITER_ELINKNO = I2S_BUFFER_TO_USE_BYTES / 2;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);

	I2S0_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
	I2S0_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE; // TX clock enable, because sync'd to TX

#elif defined(__IMXRT1062__)
	CORE_PIN8_CONFIG  = 3;  //1:RX_DATA0
	IOMUXC_SAI1_RX_DATA0_SELECT_INPUT = 2;
	
	dma.TCD->SADDR = (void *)((uint32_t)&I2S1_RDR0 + 2);
	dma.TCD->SOFF = 0;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
	dma.TCD->NBYTES_MLNO = 2;
	dma.TCD->SLAST = 0;
	dma.TCD->DADDR = i2s_rx_buffer;
	dma.TCD->DOFF = 2;
	//dma.TCD->CITER_ELINKNO = sizeof(i2s_rx_buffer) / 2; //original from Teensy Audio Library
	dma.TCD->CITER_ELINKNO = I2S_BUFFER_TO_USE_BYTES / 2;
	//dma.TCD->DLASTSGA = -sizeof(i2s_rx_buffer); //original from Teensy Audio Library
	dma.TCD->DLASTSGA = -I2S_BUFFER_TO_USE_BYTES;
	//dma.TCD->BITER_ELINKNO = sizeof(i2s_rx_buffer) / 2; //original from Teensy Audio Library
	dma.TCD->BITER_ELINKNO = I2S_BUFFER_TO_USE_BYTES / 2;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_RX);

	I2S1_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
#endif
	update_responsibility = update_setup();
	dma.enable();
	dma.attachInterrupt(isr);
	
	update_counter = 0;
}

/* void AudioInputI2S_F32::begin(bool transferUsing32bit) {
	dma.begin(true); // Allocate the DMA channel first
	
	AudioOutputI2S_F32::sample_rate_Hz = sample_rate_Hz; //these were given in the AudioSettings in the contructor
	AudioOutputI2S_F32::audio_block_samples = audio_block_samples;//these were given in the AudioSettings in the contructor
	
	//setup I2S parameters
	AudioOutputI2S_F32::config_i2s(transferUsing32bit);
		
	// TODO: should we set & clear the I2S_RCSR_SR bit here?
	CORE_PIN13_CONFIG = PORT_PCR_MUX(4); // pin 13, PTC5, I2S0_RXD0
	
	// setup DMA parameters
	//if (transferUsing32bit) {
		sub_begin_i32();
	//} else {
	//	sub_begin_i16();
	//}
	
	// finish DMA setup
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
	update_responsibility = update_setup();
	dma.enable();
	// finish I2S parameters
	I2S0_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
	I2S0_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE; // TX clock enable, because sync'd to TX
	
	//if (transferUsing32bit) {
		dma.attachInterrupt(isr_32);
	//} else {
	//	dma.attachInterrupt(isr_16);
	//}
	
	update_counter = 0;
} */

/* void AudioInputI2S_F32::sub_begin_i16(void)
{
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
}; */

/* void AudioInputI2S_F32::sub_begin_i32(void)
{
	//let's assume that we'll transfer one sample (left or right) each call.  So, it'll transfer 4 bytes (32-bits)
	dma.TCD->SADDR = (void *)((uint32_t)&I2S0_RDR0);
	dma.TCD->SOFF = 0;  //do not increment the source memory pointer
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(DMA_TCD_ATTR_SIZE_32BIT) | DMA_TCD_ATTR_DSIZE(DMA_TCD_ATTR_SIZE_32BIT);
	dma.TCD->NBYTES_MLNO = 4;  //one sample (32bits = 4bytes).  should be 4 or 8?  https://forum.pjrc.com/threads/42233-I2S-Input-Question
	dma.TCD->SLAST = 0;
	dma.TCD->DADDR = i2s_rx_buffer;
	dma.TCD->DOFF = 4;  //increment one sample (32bits = 4bytes) in the destination memory
	
	//dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;	//original, 16-bit 
	//dma.TCD->CITER_ELINKNO = I2S_BUFFER_TO_USE_BYTES / 2;   //revised WEA 16-bit
	//dma.TCD->CITER_ELINKNO = sizeof(i2s_rx_buffer_32) / 4;  //original, 32-bit
	dma.TCD->CITER_ELINKNO = I2S_BUFFER_TO_USE_BYTES / 4;   //number of minor loops in a major loop.  I2S_BUFFER_TO_USE_BYTES/NBYTES_MLNO? ...should be 4 or 8?  https://forum.pjrc.com/threads/42233-I2S-Input-Question
	
	//dma.TCD->DLASTSGA = -sizeof(i2s_rx_buffer); //original, 16-bit 
	//dma.TCD->DLASTSGA = -I2S_BUFFER_TO_USE_BYTES;//revised WEA 16-bit
	//dma.TCD->DLASTSGA = -sizeof(i2s_rx_buffer_32);//original, 32-bit
	dma.TCD->DLASTSGA = -I2S_BUFFER_TO_USE_BYTES;//revised WEA 32-bit
	
	//dma.TCD->BITER_ELINKNO = sizeof(i2s_rx_buffer) / 2;	//original, 16-bit 
	//dma.TCD->BITER_ELINKNO = I2S_BUFFER_TO_USE_BYTES / 2;//revised WEA 16-bit
	//dma.TCD->BITER_ELINKNO = sizeof(i2s_rx_buffer_32) / 4; //original, 32-bit
	dma.TCD->BITER_ELINKNO = I2S_BUFFER_TO_USE_BYTES / 4;  //number of minor loops in a major loop.  I2S_BUFFER_TO_USE_BYTES/NBYTES_MLNO?..should be 4 or 8?   https://forum.pjrc.com/threads/42233-I2S-Input-Question
	
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
}; */

/* void AudioInputI2S_F32::isr_16(void)
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
				// *dest_left++ = (int16_t)n;
				// *dest_right++ = (int16_t)(n >> 16);
				*dest_left++ = *src++;
				*dest_right++ = *src++;
			} while (src < end);
		}
	}
	//digitalWriteFast(3, LOW);
} */


void AudioInputI2S_F32::isr(void)
{
	uint32_t daddr, offset;
	const int16_t *src, *end;
	//int16_t *dest_left, *dest_right;
	//audio_block_t *left, *right;
	float32_t *dest_left_f32, *dest_right_f32;
	audio_block_f32_t *left_f32, *right_f32;

#if defined(KINETISK) || defined(__IMXRT1062__)
	daddr = (uint32_t)(dma.TCD->DADDR);
#endif
	dma.clearInterrupt();
	//Serial.println("isr");

	//if (daddr < (uint32_t)i2s_rx_buffer + sizeof(i2s_rx_buffer) / 2) { //original Teensy Audio Library
	if (daddr < (uint32_t)i2s_rx_buffer + I2S_BUFFER_TO_USE_BYTES / 2) {
		// DMA is receiving to the first half of the buffer
		// need to remove data from the second half
		//src = (int16_t *)&i2s_rx_buffer[AUDIO_BLOCK_SAMPLES/2]; //original Teensy Audio Library
		//end = (int16_t *)&i2s_rx_buffer[AUDIO_BLOCK_SAMPLES]; //original Teensy Audio Library
		src = (int16_t *)&i2s_rx_buffer[audio_block_samples/2];
		end = (int16_t *)&i2s_rx_buffer[audio_block_samples];			
		update_counter++; //let's increment the counter here to ensure that we get every ISR resulting in audio
		if (AudioInputI2S_F32::update_responsibility) AudioStream_F32::update_all();
	} else {
		// DMA is receiving to the second half of the buffer
		// need to remove data from the first half
		src = (int16_t *)&i2s_rx_buffer[0];
		//end = (int16_t *)&i2s_rx_buffer[AUDIO_BLOCK_SAMPLES/2]; //original Teensy Audio Library
		end = (int16_t *)&i2s_rx_buffer[audio_block_samples/2];
	}
	left_f32 = AudioInputI2S_F32::block_left_f32;
	right_f32 = AudioInputI2S_F32::block_right_f32;
	if (left_f32 != NULL && right_f32 != NULL) {
		offset = AudioInputI2S_F32::block_offset;
		//if (offset <= (uint32_t)(AUDIO_BLOCK_SAMPLES/2)) { //original Teensy Audio Library
		if (offset <= ((uint32_t) audio_block_samples/2)) {
			dest_left_f32 = &(left_f32->data[offset]);
			dest_right_f32 = &(right_f32->data[offset]);
			//AudioInputI2S_F32::block_offset = offset + AUDIO_BLOCK_SAMPLES/2; //original Teensy Audio Library
			AudioInputI2S_F32::block_offset = offset + audio_block_samples/2;
			do {
				//Serial.println(*src);
				//n = *src++;
				//*dest_left++ = (int16_t)n;
				//*dest_right++ = (int16_t)(n >> 16);
				*dest_left_f32++ = (float32_t) *src++;
				*dest_right_f32++ = (float32_t) *src++;
			} while (src < end);
		}
	}
}

/* void AudioInputI2S_F32::isr_32(void)
{
	static bool flag_beenSuccessfullOnce = false;
	uint32_t daddr, offset;
	const int32_t *src_i32, *end_i32;
	//int16_t *dest_left, *dest_right;
	float32_t *dest_left_f32, *dest_right_f32;
	audio_block_f32_t *left_f32, *right_f32;
	daddr = (uint32_t)(dma.TCD->DADDR);
	
	dma.clearInterrupt();
	//if (daddr < (uint32_t)i2s_rx_buffer + sizeof(i2s_rx_buffer) / 2) {
	if (daddr < (uint32_t)i2s_rx_buffer + I2S_BUFFER_TO_USE_BYTES / 2) {		
		
		// DMA is receiving to the first half of the buffer
		// need to remove data from the second half
		//src = (int32_t *)&i2s_rx_buffer_32[AUDIO_BLOCK_SAMPLES];
		//end = (int32_t *)&i2s_rx_buffer_32[AUDIO_BLOCK_SAMPLES*2];
		src_i32 = (int32_t *)&i2s_rx_buffer[audio_block_samples];	//WEA revised
		end_i32 = (int32_t *)&i2s_rx_buffer[audio_block_samples*2];	//WEA revised
		update_counter++; //let's increment the counter here to ensure that we get every ISR resulting in audio
		if (AudioInputI2S_F32::update_responsibility) AudioStream_F32::update_all();
		
	} else {
		// DMA is receiving to the second half of the buffer
		// need to remove data from the first half
		//src = (int32_t *)&i2s_rx_buffer_32[0];
		//end = (int32_t *)&i2s_rx_buffer_32[AUDIO_BLOCK_SAMPLES];		
		src_i32 = (int32_t *)&i2s_rx_buffer[0];
		end_i32 = (int32_t *)&i2s_rx_buffer[audio_block_samples];		
	}
	
	// OLD COMMENT: extract 16 but from 32 bit I2S buffer but shift to right first
    // OLD COMMENT: there will be two buffers with each having "AUDIO_BLOCK_SAMPLES" samples
	left_f32 = AudioInputI2S_F32::block_left_f32;
	right_f32 = AudioInputI2S_F32::block_right_f32;
	if ((left_f32 != NULL) && (right_f32 != NULL)) {
		offset = AudioInputI2S_F32::block_offset;
		//if (offset <= AUDIO_BLOCK_SAMPLES/2) {	//original
		if (offset <= ((uint32_t) audio_block_samples/2)) {
			dest_left_f32 = &(left_f32->data[offset]);
			dest_right_f32 = &(right_f32->data[offset]);
			//AudioInputI2S_F32::block_offset = offset + AUDIO_BLOCK_SAMPLES/2;	//original
			AudioInputI2S_F32::block_offset = offset + audio_block_samples/2;
			do {
				//n = *src++;
				// *dest_left++ = (int16_t)n;
				// *dest_right++ = (int16_t)(n >> 16);
				*dest_left_f32++ = (float32_t) *src_i32++; 
				*dest_right_f32++ = (float32_t) *src_i32++;
			} while (src_i32 < end_i32);
		}
		flag_beenSuccessfullOnce = true;
	} else {
		if (flag_beenSuccessfullOnce) {
			//but we were not successful this time
			Serial.println("Input I2S: isr_32: WARNING!!! Null memory block.");
		}
	}
} */

#define I16_TO_F32_NORM_FACTOR (3.051850947599719e-05)  //which is 1/32767 
void AudioInputI2S_F32::scale_i16_to_f32( float32_t *p_i16, float32_t *p_f32, int len) {
	for (int i=0; i<len; i++) { *p_f32++ = ((*p_i16++) * I16_TO_F32_NORM_FACTOR); }
}
#define I24_TO_F32_NORM_FACTOR (1.192093037616377e-07)   //which is 1/(2^23 - 1)
void AudioInputI2S_F32::scale_i24_to_f32( float32_t *p_i24, float32_t *p_f32, int len) {
	for (int i=0; i<len; i++) { *p_f32++ = ((*p_i24++) * I24_TO_F32_NORM_FACTOR); }
}
#define I32_TO_F32_NORM_FACTOR (4.656612875245797e-10)   //which is 1/(2^31 - 1)
void AudioInputI2S_F32::scale_i32_to_f32( float32_t *p_i32, float32_t *p_f32, int len) {
	for (int i=0; i<len; i++) { *p_f32++ = ((*p_i32++) * I32_TO_F32_NORM_FACTOR); }
}


/* void AudioInputI2S_F32::update_i16(void)
{
	audio_block_t *new_left=NULL, *new_right=NULL, *out_left=NULL, *out_right=NULL;
	// allocate 2 new blocks, but if one fails, allocate neither
	new_left = AudioStream::allocate();
	if (new_left != NULL) {
		new_right = AudioStream::allocate();
		if (new_right == NULL) {
			flag_out_of_memory = 1;
			AudioStream::release(new_left);
			new_left = NULL;
		}
	} else {
		flag_out_of_memory = 1;
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
				flag_out_of_memory = 2;
				AudioStream_F32::release(out_left_f32);
				out_left_f32 = NULL;
			}
		} else {
			flag_out_of_memory = 2;
		}
		if (out_left_f32 != NULL) {
			//convert int16 to float 32
			scale_i16_to_f32(out_left->data, out_left_f32->data, audio_block_samples);
			scale_i16_to_f32(out_right->data, out_right_f32->data, audio_block_samples);
			
			//prepare to transmit
			update_counter++;
			out_left_f32->id = update_counter;
			out_right_f32->id = update_counter;
			
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
 */
 
 void AudioInputI2S_F32::update_1chan(int chan, audio_block_f32_t *&out_f32) {
	 if (!out_f32) return;
	 
	//scale the float values so that the maximum possible audio values span -1.0 to + 1.0
	//scale_i32_to_f32(out_f32->data, out_f32->data, audio_block_samples);
	scale_i16_to_f32(out_f32->data, out_f32->data, audio_block_samples);

	//prepare to transmit by setting the update_counter (which helps tell if data is skipped or out-of-order)
	out_f32->id = update_counter;
		
	//transmit the f32 data!
	AudioStream_F32::transmit(out_f32,chan);

	//release the memory blocks
	AudioStream_F32::release(out_f32);
}
 
void AudioInputI2S_F32::update(void)
{
	static bool flag_beenSuccessfullOnce = false;
	audio_block_f32_t *new_left=NULL, *new_right=NULL, *out_left=NULL, *out_right=NULL;
	
	new_left = AudioStream_F32::allocate_f32();
	new_right = AudioStream_F32::allocate_f32();
	if ((!new_left) || (!new_right)) {
		//ran out of memory.  Clear and return!
		if (new_left) AudioStream_F32::release(new_left);
		if (new_right) AudioStream_F32::release(new_right);
		new_left = NULL;  new_right = NULL;
		flag_out_of_memory = 1;
		if (flag_beenSuccessfullOnce) Serial.println("Input_I2S_F32: update(): WARNING!!! Out of Memory.");
	} else {
		flag_beenSuccessfullOnce = true;
	}
	
	__disable_irq();
	if (block_offset >= audio_block_samples) {
		// the DMA filled 2 blocks, so grab them and get the
		// 2 new blocks to the DMA, as quickly as possible
		out_left = block_left_f32;
		block_left_f32 = new_left;
		out_right = block_right_f32;
		block_right_f32 = new_right;
		block_offset = 0;
		__enable_irq();
		
		//update_counter++; //I chose to update it in the ISR instead.
		update_1chan(0,out_left);  //uses audio_block_samples and update_counter
		update_1chan(1,out_right);  //uses audio_block_samples and update_counter
		
		
	} else if (new_left != NULL) {
		// the DMA didn't fill blocks, but we allocated blocks
		if (block_left_f32 == NULL) {
			// the DMA doesn't have any blocks to fill, so
			// give it the ones we just allocated
			block_left_f32 = new_left;
			block_right_f32 = new_right;
			block_offset = 0;
			__enable_irq();
		} else {
			// the DMA already has blocks, doesn't need these
			__enable_irq();
			AudioStream_F32::release(new_left);
			AudioStream_F32::release(new_right);
		}
	} else {
		// The DMA didn't fill blocks, and we could not allocate
		// memory... the system is likely starving for memory!
		// Sadly, there's nothing we can do.
		__enable_irq();
	}
}

/******************************************************************/


void AudioInputI2Sslave_F32::begin(void)
{
	dma.begin(true); // Allocate the DMA channel first

	//block_left_1st = NULL;
	//block_right_1st = NULL;

	AudioOutputI2Sslave_F32::config_i2s();
#if defined(KINETISK)
	CORE_PIN13_CONFIG = PORT_PCR_MUX(4); // pin 13, PTC5, I2S0_RXD0

	dma.TCD->SADDR = (void *)((uint32_t)&I2S0_RDR0 + 2);
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

	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
	update_responsibility = update_setup();
	dma.enable();

	I2S0_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
	I2S0_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE; // TX clock enable, because sync'd to TX
	dma.attachInterrupt(isr);
#endif	
}

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
