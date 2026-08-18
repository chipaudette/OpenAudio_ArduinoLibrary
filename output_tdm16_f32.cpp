/*
 *      output_tdm16_f32.cpp
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
 *  Float samples in [-1, +1] are scaled to int32 and written into the
 *  32-bit TDM slots (24-bit left-justified codecs take the top 24 bits).
 */

#include <Arduino.h>

#if defined(__IMXRT1062__)

#include "output_tdm16_f32.h"

// 2x buffer for ping-pong: DMA transmits one half while the ISR fills the other
DMAMEM __attribute__((aligned(32)))
static uint32_t tdm_tx_buffer[AUDIO_BLOCK_SAMPLES * 16 * 2]; // 16 channels, one 32-bit slot each
DMAMEM __attribute__((aligned(32)))
static float zeros_f32[AUDIO_BLOCK_SAMPLES];
audio_block_f32_t * AudioOutputTDM16_F32::block_input[16] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
bool AudioOutputTDM16_F32::update_responsibility = false;
DMAChannel AudioOutputTDM16_F32::dma(false);
float AudioOutputTDM16_F32::sample_rate_Hz = AUDIO_SAMPLE_RATE;
int AudioOutputTDM16_F32::audio_block_samples = AUDIO_BLOCK_SAMPLES;

#define F32_TO_I32_NORM_FACTOR (2147483647.0f)   //which is 2^31 - 1

void AudioOutputTDM16_F32::begin(void)
{
	dma.begin(true); // Allocate the DMA channel first

	for (int i = 0; i < 16; i++) {
		block_input[i] = NULL;
	}
	memset(zeros_f32, 0, sizeof(zeros_f32));
	memset(tdm_tx_buffer, 0, sizeof(tdm_tx_buffer));

	config_tdm();

	CORE_PIN7_CONFIG = 3;  //1:TX_DATA0

	dma.TCD->SADDR = tdm_tx_buffer;
	dma.TCD->SOFF = 4;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
	dma.TCD->NBYTES_MLNO = 4;
	dma.TCD->SLAST = -((int32_t)(audio_block_samples * 16 * 2 * 4));
	dma.TCD->DADDR = &I2S1_TDR0;
	dma.TCD->DOFF = 0;
	dma.TCD->CITER_ELINKNO = audio_block_samples * 16 * 2;
	dma.TCD->DLASTSGA = 0;
	dma.TCD->BITER_ELINKNO = audio_block_samples * 16 * 2;
	// one interrupt per half buffer = one full block per channel per interrupt.
	// The ISR always fills the half the DMA is NOT currently transmitting, so
	// the DMA never reads a half while it is being rewritten (no tearing).
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_TX);

	update_responsibility = update_setup();
	dma.enable();

	I2S1_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE;
	I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;
	dma.attachInterrupt(isr);
}

void AudioOutputTDM16_F32::isr(void)
{
	uint32_t *dest;
	unsigned int ch;
	uint32_t saddr;

	saddr = (uint32_t)(dma.TCD->SADDR);
	dma.clearInterrupt();

	if (saddr < (uint32_t)tdm_tx_buffer + sizeof(tdm_tx_buffer) / 2) {
		// DMA is transmitting the first half, so we fill the second half
		dest = tdm_tx_buffer + audio_block_samples * 16;
	} else {
		// DMA is transmitting the second half, so we fill the first half
		dest = tdm_tx_buffer;
	}

	if (update_responsibility) AudioStream_F32::update_all();

	// interleave: channel ch lives in slot ch of every 16-slot frame
	for (ch = 0; ch < 16; ch++) {
		audio_block_f32_t *b = block_input[ch];
		const float *src = b ? b->data : zeros_f32;
		int32_t *d = (int32_t *)dest + ch;
		for (int i = 0; i < audio_block_samples; i++) {
			float s = *src++;
			if (s > 1.0f) s = 1.0f;
			else if (s < -1.0f) s = -1.0f;
			*d = (int32_t)(s * F32_TO_I32_NORM_FACTOR);
			d += 16;
		}
	}

	#if IMXRT_CACHE_ENABLED >= 2
	arm_dcache_flush_delete(dest, sizeof(tdm_tx_buffer) / 2);
	#endif

	for (ch = 0; ch < 16; ch++) {
		if (block_input[ch]) {
			AudioStream_F32::release(block_input[ch]);
			block_input[ch] = NULL;
		}
	}
}

void AudioOutputTDM16_F32::update(void)
{
	audio_block_f32_t *prev[16];
	unsigned int i;

	__disable_irq();
	for (i = 0; i < 16; i++) {
		prev[i] = block_input[i];
		block_input[i] = receiveReadOnly_f32(i);
	}
	__enable_irq();
	for (i = 0; i < 16; i++) {
		if (prev[i]) AudioStream_F32::release(prev[i]);
	}
}

void AudioOutputTDM16_F32::config_tdm(void)
{
	CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);

	// if either transmitter or receiver is enabled, do nothing
	if (I2S1_TCSR & I2S_TCSR_TE) return;
	if (I2S1_RCSR & I2S_RCSR_RE) return;

	int fs = AUDIO_SAMPLE_RATE_EXACT;
	// PLL between 27*24 = 648MHz und 54*24=1296MHz
	int n1 = 4; //SAI prescaler 4 => (n1*n2) = multiple of 4
	int n2 = 1 + (24000000 * 27) / (fs * 256 * n1);

	double C = ((double)fs * 256 * n1 * n2) / 24000000;
	int c0 = C;
	int c2 = 10000;
	int c1 = C * c2 - (c0 * c2);
	set_audioClock(c0, c1, c2);
	// clear SAI1_CLK register locations
	CCM_CSCMR1 = (CCM_CSCMR1 & ~(CCM_CSCMR1_SAI1_CLK_SEL_MASK))
		   | CCM_CSCMR1_SAI1_CLK_SEL(2); // &0x03 // (0,1,2): PLL3PFD0, PLL5, PLL4

	n1 = 1; // TDM16: 16 slots x 32 bits = 512 bit clocks per frame (2x the TDM8 clock)

	CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
		   | CCM_CS1CDR_SAI1_CLK_PRED(n1-1) // &0x07
		   | CCM_CS1CDR_SAI1_CLK_PODF(n2-1); // &0x3f

	IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~(IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK))
			| (IOMUXC_GPR_GPR1_SAI1_MCLK_DIR | IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL(0));	//Select MCLK

	// configure transmitter
	int rsync = 0;
	int tsync = 1;

	I2S1_TMR = 0;
	I2S1_TCR1 = I2S_TCR1_RFW(4);
	I2S1_TCR2 = I2S_TCR2_SYNC(tsync) | I2S_TCR2_BCP | I2S_TCR2_MSEL(1)
		| I2S_TCR2_BCD | I2S_TCR2_DIV(0);
	I2S1_TCR3 = I2S_TCR3_TCE;
	I2S1_TCR4 = I2S_TCR4_FRSZ(15) | I2S_TCR4_SYWD(0) | I2S_TCR4_MF
		| I2S_TCR4_FSE | I2S_TCR4_FSD;
	I2S1_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

	I2S1_RMR = 0;
	I2S1_RCR1 = I2S_RCR1_RFW(4);
	I2S1_RCR2 = I2S_RCR2_SYNC(rsync) | I2S_TCR2_BCP | I2S_RCR2_MSEL(1)
		| I2S_RCR2_BCD | I2S_RCR2_DIV(0);
	I2S1_RCR3 = I2S_RCR3_RCE;
	I2S1_RCR4 = I2S_RCR4_FRSZ(15) | I2S_RCR4_SYWD(0) | I2S_RCR4_MF
		| I2S_RCR4_FSE | I2S_RCR4_FSD;
	I2S1_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

	CORE_PIN23_CONFIG = 3;  //1:MCLK
	CORE_PIN21_CONFIG = 3;  //1:RX_BCLK
	CORE_PIN20_CONFIG = 3;  //1:RX_SYNC
}

#endif
