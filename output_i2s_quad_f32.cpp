/*
 *  ***** output_i2s_quad_f32.cpp  *****
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
 *  The F32 conversion is under the MIT License.  Use at your own risk.
 */
// Updated OpenAudio F32 with this version from Chip Audette's Tympan Library Jan 2021 RSL
// Removed old commented out code.  RSL 30 May 2022
// This version is 4 channel I2S output.  Greg Raven KF5N October, 2025

#include "output_i2s_quad_f32.h"

DMAChannel AudioOutputI2SQuad_F32::dma(false);
DMAMEM __attribute__((aligned(32))) static float32_t i2s_tx_buffer[AUDIO_BLOCK_SAMPLES * 4]; // 4 channels 128 samples each, total = 512.

void AudioOutputI2SQuad_F32::begin()
{
    // Configure most of the I2S.
    AudioOutputI2SQuad_F32::config_i2s(sample_rate_Hz);

    // Configure the I2S output pins.
    CORE_PIN7_CONFIG = 3;  // Teensy pin 7 is 1st and 2nd channels of I2S (Audio Adapter on Teensy 4.1).
    CORE_PIN32_CONFIG = 3; // Teensy pin 32 is 3rd and 4th channels of I2S.

    // Zero the transmit buffer.
    memset(i2s_tx_buffer, 0, sizeof(i2s_tx_buffer));

    // Configure the DMA channel.
    dma.begin(true); // Allocate the DMA channel.
    dma.TCD->SADDR = i2s_tx_buffer;
    dma.TCD->SOFF = 4;
    dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
    dma.TCD->NBYTES_MLOFFYES = DMA_TCD_NBYTES_DMLOE |
                               DMA_TCD_NBYTES_MLOFFYES_MLOFF(-8) |
                               DMA_TCD_NBYTES_MLOFFYES_NBYTES(8);
    dma.TCD->SLAST = -sizeof(i2s_tx_buffer);
    dma.TCD->DOFF = 4;
    dma.TCD->CITER_ELINKNO = 256;
    dma.TCD->DLASTSGA = -8;
    dma.TCD->BITER_ELINKNO = 256;
    dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
    dma.TCD->DADDR = (void *)((uint32_t)&I2S1_TDR0); // I2S1 address.
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_TX);
    dma.attachInterrupt(AudioOutputI2SQuad_F32::isr);
    update_responsibility = update_setup();
    dma.enable();
    enabled = 1; // What is this?

    // Configure the rest of the I2S unique to quad output.
    I2S1_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE;
    I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;
    I2S1_TCR3 = I2S_TCR3_TCE_2CH;
}

// Interrupt service routine called twice per update by the DMA.
// This method takes the data retrieved by update() and interleaves
// it such that the DMA copies to the I2S in the correct order.
void AudioOutputI2SQuad_F32::isr(void)
{
    dma.clearInterrupt();
    int32_t *dest = nullptr;
    uint32_t saddr;
    int32_t offset;

    // Get the current source address from the DMA.
    // This will tell us if the DMA is at halfway or the end.
    saddr = (uint32_t)(dma.TCD->SADDR);

    if (saddr < (uint32_t)i2s_tx_buffer + sizeof(i2s_tx_buffer) / 2)
    {
        // DMA is transmitting the first half of the buffer.
        // dest points to the middle of i2s_tx_buffer.
        dest = (int32_t *)&i2s_tx_buffer[audio_block_samples * 2];
        offset = half_block_length;
        if (AudioOutputI2SQuad_F32::update_responsibility)
            AudioStream_F32::update_all();
    }
    else
    {
        // DMA is transmitting the second half of the buffer.
        // dest points to the beginning of i2s_tx_buffer.
        dest = (int32_t *)i2s_tx_buffer;
        offset = 0;
    }

    /* Debugging code.
    if (block_left_1st)
        Serial.printf("block_left_1st\n");
    if (block_right_1st)
        Serial.printf("block_right_1st\n");
    if (block_left_2nd)
        Serial.printf("block_left_2nd\n");
    if (block_right_2nd)
        Serial.printf("block_right_2nd\n");
    */

    // "Ping pong" between loading data into first half and second half of i2s_tx_buffer.
    // The DMA will be transmitting the other half while this isr() runs.
    // The samples are interleaved into i2s_tx_buffer in order for the DMA to
    // transfer the sample to the correct register of the I2S.

    // First left and right channels.

    // When both inputs are connected to data sources.
    if (block_left_1st && block_right_1st)
    {
        for (int i = 0, j = 0; i < half_block_length && j < half_buffer_length; i = i + 1, j = j + 4)
        {
            dest[j] = block_left_1st->data[i + offset];
            dest[j + 2] = block_right_1st->data[i + offset];
        }
    }
    //  Only the left input has something connected.
    else if (block_left_1st)
    {
        for (int i = 0, j = 0; i < 64 && j < 256; i = i + 1, j = j + 4)
        {
            dest[j] = block_left_1st->data[i + offset];
        }
    }
    // Only the right input has something connected.
    else if (block_right_1st)
    {
        for (int i = 0, j = 0; i < half_block_length && j < half_buffer_length; i = i + 1, j = j + 4)
        {
            dest[j + 2] = block_right_1st->data[i + offset];
        }
    }

    // Second left and right channels.

    // When both inputs are connected to data sources.
    if (block_left_2nd && block_right_2nd)
    {
        for (int i = 0, j = 0; i < half_block_length && j < half_buffer_length; i = i + 1, j = j + 4)
        {
            dest[j + 1] = block_left_2nd->data[i + offset];
            dest[j + 3] = block_right_2nd->data[i + offset];
        }
    }
    //  Only the left input has something connected.
    else if (block_left_2nd)
    {
        for (int i = 0, j = 0; i < half_block_length && j < half_buffer_length; i = i + 1, j = j + 4)
        {
            dest[j + 1] = block_left_2nd->data[i + offset];
        }
    }
    // Only the right input has something connected.
    else if (block_right_2nd)
    {
        for (int i = 0, j = 0; i < half_block_length && j < half_buffer_length; i = i + 1, j = j + 4)
        {
            dest[j + 3] = block_right_2nd->data[i + offset];
        }
    }

    // If there is no data, zero out the half of the buffer being processed.
    if (not block_left_1st && not block_right_1st && not block_left_2nd && not block_right_2nd)
        memset(dest, 0, sizeof(i2s_tx_buffer) / 2);

    // Flush the L1 cache so that the data fetched by the DMA is accurate.
    arm_dcache_flush_delete(dest, sizeof(i2s_tx_buffer) / 2);

    // Release memory and null pointers as required.

    dest = nullptr;

    // First left and first right channels.
    if (block_left_1st && offset == half_block_length)
    {
        AudioStream_F32::release(block_left_1st);
        AudioOutputI2SQuad_F32::block_left_1st = nullptr;
    }
    if (block_right_1st && offset == half_block_length)
    {
        AudioStream_F32::release(block_right_1st);
        AudioOutputI2SQuad_F32::block_right_1st = nullptr;
    }

    // Second left and right channels.
    if (block_left_2nd && offset == half_block_length)
    {
        AudioStream_F32::release(block_left_2nd);
        AudioOutputI2SQuad_F32::block_left_2nd = nullptr;
    }
    if (block_right_2nd && offset == half_block_length)
    {
        AudioStream_F32::release(block_right_2nd);
        AudioOutputI2SQuad_F32::block_right_2nd = nullptr;
    }
}

// Scale the floating point data to integer format used by the data converters.
// This method modifies the given array in place.
void AudioOutputI2SQuad_F32::scale_f32_to_i32(float32_t *p_f32, int len)
{
    for (int i = 0; i < len; i++)
    {
        p_f32[i] = max(-F32_TO_I32_NORM_FACTOR, min(F32_TO_I32_NORM_FACTOR, (p_f32[i]) * F32_TO_I32_NORM_FACTOR));
    }
}

//  Read data blocks from the inputs.
//  Scale amplitude by outputScale and translate to I32 for DACs.
//  Do nothing if there is no data available.
//  Update is called once for every two calls of isr().
void AudioOutputI2SQuad_F32::update(void)
{
    block_left_1st = receiveWritable_f32(0); // Input 1 is first left channel.
    if (block_left_1st)
    {
        __disable_irq();
        if (outputScale < 1.0f || outputScale > 1.0f)
            arm_scale_f32(block_left_1st->data, outputScale, block_left_1st->data, block_left_1st->length);
        scale_f32_to_i32(block_left_1st->data, audio_block_samples);
        __enable_irq();
    }

    block_right_1st = receiveWritable_f32(1); // Input 2 is first right channel.
    if (block_right_1st)
    {
        __disable_irq();
        if (outputScale < 1.0f || outputScale > 1.0f)
            arm_scale_f32(block_right_1st->data, outputScale, block_right_1st->data, block_right_1st->length);
        scale_f32_to_i32(block_right_1st->data, audio_block_samples);
        __enable_irq();
    }

    block_left_2nd = receiveWritable_f32(2); // Input 2 is second left channel.
    if (block_left_2nd)
    {
        __disable_irq();
        if (outputScale < 1.0f || outputScale > 1.0f)
            arm_scale_f32(block_left_2nd->data, outputScale, block_left_2nd->data, block_left_2nd->length);
        scale_f32_to_i32(block_left_2nd->data, audio_block_samples);
        __enable_irq();
    }

    block_right_2nd = receiveWritable_f32(3); // Input 4 = 2nd right channel
    if (block_right_2nd)
    {
        __disable_irq();
        if (outputScale < 1.0f || outputScale > 1.0f)
            arm_scale_f32(block_right_2nd->data, outputScale, block_right_2nd->data, block_right_2nd->length);
        scale_f32_to_i32(block_right_2nd->data, audio_block_samples);
        __enable_irq();
    }
}

// Configure the I2S peripheral.  This is most, but not all, of the configuration.  The rest is done by begin().
void AudioOutputI2SQuad_F32::config_i2s(int fs_Hz)
{
    CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);

    // if either transmitter or receiver is enabled, do nothing
    if (I2S1_TCSR & I2S_TCSR_TE)
        return;
    if (I2S1_RCSR & I2S_RCSR_RE)
        return;
    // PLL:
    int fs = fs_Hz;

    // PLL between 27*24 = 648MHz und 54*24=1296MHz
    int n1 = 4; // SAI prescaler 4 => (n1*n2) = multiple of 4
    int n2 = 1 + (24000000 * 27) / (fs * 256 * n1);

    double C = ((double)fs * 256 * n1 * n2) / 24000000;
    int c0 = C;
    int c2 = 10000;
    int c1 = C * c2 - (c0 * c2);
    set_audioClock(c0, c1, c2);

    // clear SAI1_CLK register locations
    CCM_CSCMR1 = (CCM_CSCMR1 & ~(CCM_CSCMR1_SAI1_CLK_SEL_MASK)) | CCM_CSCMR1_SAI1_CLK_SEL(2);                                       // &0x03 // (0,1,2): PLL3PFD0, PLL5, PLL4
    CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK)) | CCM_CS1CDR_SAI1_CLK_PRED(n1 - 1) // &0x07
                 | CCM_CS1CDR_SAI1_CLK_PODF(n2 - 1);                                                                                // &0x3f

    // Select MCLK
    IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~(IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK)) | (IOMUXC_GPR_GPR1_SAI1_MCLK_DIR | IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL(0));

    CORE_PIN23_CONFIG = 3; // 1:MCLK
    CORE_PIN21_CONFIG = 3; // 1:RX_BCLK
    CORE_PIN20_CONFIG = 3; // 1:RX_SYNC

    int rsync = 0;
    int tsync = 1;

    I2S1_TMR = 0;
    // I2S1_TCSR = (1<<25); //Reset
    I2S1_TCR1 = I2S_TCR1_RFW(1);
    I2S1_TCR2 = I2S_TCR2_SYNC(tsync) | I2S_TCR2_BCP // sync=0; tx is async;
                | (I2S_TCR2_BCD | I2S_TCR2_DIV((1)) | I2S_TCR2_MSEL(1));
    I2S1_TCR3 = I2S_TCR3_TCE;
    I2S1_TCR4 = I2S_TCR4_FRSZ((2 - 1)) | I2S_TCR4_SYWD((32 - 1)) | I2S_TCR4_MF | I2S_TCR4_FSD | I2S_TCR4_FSE | I2S_TCR4_FSP;
    I2S1_TCR5 = I2S_TCR5_WNW((32 - 1)) | I2S_TCR5_W0W((32 - 1)) | I2S_TCR5_FBT((32 - 1));

    I2S1_RMR = 0;
    // I2S1_RCSR = (1<<25); //Reset
    I2S1_RCR1 = I2S_RCR1_RFW(1);
    I2S1_RCR2 = I2S_RCR2_SYNC(rsync) | I2S_RCR2_BCP // sync=0; rx is async;
                | (I2S_RCR2_BCD | I2S_RCR2_DIV((1)) | I2S_RCR2_MSEL(1));
    I2S1_RCR3 = I2S_RCR3_RCE;
    I2S1_RCR4 = I2S_RCR4_FRSZ((2 - 1)) | I2S_RCR4_SYWD((32 - 1)) | I2S_RCR4_MF | I2S_RCR4_FSE | I2S_RCR4_FSP | I2S_RCR4_FSD;
    I2S1_RCR5 = I2S_RCR5_WNW((32 - 1)) | I2S_RCR5_W0W((32 - 1)) | I2S_RCR5_FBT((32 - 1));
}

// From Chip: The I2SSlave functionality has NOT been extended to
// allow for different block sizes or sample rates (2020-10-31)
// Quad slave object not working yet!  Greg Raven KF5N

void AudioOutputI2SQuadslave_F32::begin(void)
{
    dma.begin(true); // Allocate the DMA channel first

    AudioOutputI2SQuadslave_F32::config_i2s();

#if defined(KINETISK)
    CORE_PIN22_CONFIG = PORT_PCR_MUX(6); // pin 22, PTC1, I2S0_TXD0
    dma.TCD->SADDR = i2s_tx_buffer;
    dma.TCD->SOFF = 2;
    dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    dma.TCD->NBYTES_MLNO = 2; // Only the right input has something connected.
    dma.TCD->SLAST = -sizeof(i2s_tx_buffer);
    dma.TCD->DADDR = (void *)((uint32_t)&I2S0_TDR0 + 2);
    dma.TCD->DOFF = 0;
    dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
    dma.TCD->DLASTSGA = 0;
    dma.TCD->BITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
    dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_TX);
    dma.enable();

    I2S0_TCSR = I2S_TCSR_SR;
    I2S0_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;

#elif defined(__IMXRT1062__)
    CORE_PIN7_CONFIG = 3; // 1:TX_DATA0
    dma.TCD->SADDR = i2s_tx_buffer;
    dma.TCD->SOFF = 2;
    dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    dma.TCD->NBYTES_MLNO = 2;
    dma.TCD->SLAST = -sizeof(i2s_tx_buffer);
    // dma.TCD->DADDR = (void *)((uint32_t)&I2S1_TDR1 + 2);
    dma.TCD->DOFF = 0;
    dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
    dma.TCD->DLASTSGA = 0;
    dma.TCD->BITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
    // dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI2_TX);
    dma.TCD->DADDR = (void *)((uint32_t)&I2S1_TDR0 + 2);
    dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_TX);
    dma.enable();

    I2S1_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE;
    I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;

#endif

    update_responsibility = update_setup();
    // dma.enable();
    dma.attachInterrupt(AudioOutputI2SQuad_F32::isr);
}

void AudioOutputI2SQuadslave_F32::config_i2s(void)
{
#if defined(KINETISK)
    SIM_SCGC6 |= SIM_SCGC6_I2S;
    SIM_SCGC7 |= SIM_SCGC7_DMA;
    SIM_SCGC6 |= SIM_SCGC6_DMAMUX;

    // if either transmitter or receiver is enabled, do nothing
    if (I2S0_TCSR & I2S_TCSR_TE)
        return;
    if (I2S0_RCSR & I2S_RCSR_RE)
        return;

    // Select input clock 0
    // Configure to input the bit-clock from pin, bypasses the MCLK divider
    I2S0_MCR = I2S_MCR_MICS(0);
    I2S0_MDR = 0;

    // configure transmitter
    I2S0_TMR = 0;
    I2S0_TCR1 = I2S_TCR1_TFW(1); // watermark at half fifo size
    I2S0_TCR2 = I2S_TCR2_SYNC(0) | I2S_TCR2_BCP;

    I2S0_TCR3 = I2S_TCR3_TCE;
    I2S0_TCR4 = I2S_TCR4_FRSZ(1) | I2S_TCR4_SYWD(31) | I2S_TCR4_MF | I2S_TCR4_FSE | I2S_TCR4_FSP;

    I2S0_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

    // configure receiver (sync'd to transmitter clocks)
    I2S0_RMR = 0;
    I2S0_RCR1 = I2S_RCR1_RFW(1);
    I2S0_RCR2 = I2S_RCR2_SYNC(1) | I2S_TCR2_BCP;

    I2S0_RCR3 = I2S_RCR3_RCE;
    I2S0_RCR4 = I2S_RCR4_FRSZ(1) | I2S_RCR4_SYWD(31) | I2S_RCR4_MF | I2S_RCR4_FSE | I2S_RCR4_FSP | I2S_RCR4_FSD;

    I2S0_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

    // configure pin mux for 3 clock signals
    CORE_PIN23_CONFIG = PORT_PCR_MUX(6); // pin 23, PTC2, I2S0_TX_FS (LRCLK)
    CORE_PIN9_CONFIG = PORT_PCR_MUX(6);  // pin  9, PTC3, I2S0_TX_BCLK
    CORE_PIN11_CONFIG = PORT_PCR_MUX(6); // pin 11, PTC6, I2S0_MCLK

#elif defined(__IMXRT1062__)

    CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);

    // if either transmitter or receiver is enabled, do nothing
    if (I2S1_TCSR & I2S_TCSR_TE)
        return;
    if (I2S1_RCSR & I2S_RCSR_RE)
        return;

    // not using MCLK in slave mode - hope that's ok?
    // CORE_PIN23_CONFIG = 3;  // AD_B1_09  ALT3=SAI1_MCLK
    CORE_PIN21_CONFIG = 3;                // AD_B1_11  ALT3=SAI1_RX_BCLK
    CORE_PIN20_CONFIG = 3;                // AD_B1_10  ALT3=SAI1_RX_SYNC
    IOMUXC_SAI1_RX_BCLK_SELECT_INPUT = 1; // 1=GPIO_AD_B1_11_ALT3, page 868
    IOMUXC_SAI1_RX_SYNC_SELECT_INPUT = 1; // 1=GPIO_AD_B1_10_ALT3, page 872

    // configure transmitter
    I2S1_TMR = 0;
    I2S1_TCR1 = I2S_TCR1_RFW(1); // watermark at half fifo size
    I2S1_TCR2 = I2S_TCR2_SYNC(1) | I2S_TCR2_BCP;
    I2S1_TCR3 = I2S_TCR3_TCE;
    I2S1_TCR4 = I2S_TCR4_FRSZ(1) | I2S_TCR4_SYWD(31) | I2S_TCR4_MF | I2S_TCR4_FSE | I2S_TCR4_FSP | I2S_RCR4_FSD;
    I2S1_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

    // configure receiver
    I2S1_RMR = 0;
    I2S1_RCR1 = I2S_RCR1_RFW(1);
    I2S1_RCR2 = I2S_RCR2_SYNC(0) | I2S_TCR2_BCP;
    I2S1_RCR3 = I2S_RCR3_RCE;
    I2S1_RCR4 = I2S_RCR4_FRSZ(1) | I2S_RCR4_SYWD(31) | I2S_RCR4_MF | I2S_RCR4_FSE | I2S_RCR4_FSP;
    I2S1_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

#endif
}
