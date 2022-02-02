/*
 *   AudioLMSDenoiseNotch_F32
 *
 * 22 January 2022   copyright (c)Robert Larkin 2022
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

#include "AudioStream_F32.h"
#include "AudioLMSDenoiseNotch_F32.h"

void AudioLMSDenoiseNotch_F32::update(void)
    {
    uint16_t j, k;
    float32_t blockDataIn, error, firOut;
    audio_block_f32_t *block;

    block = AudioStream_F32::receiveWritable_f32();
    if (!block) return;

    if(!doLMS)
        {
        AudioStream_F32::transmit(block);
        AudioStream_F32::release(block);
        return;
        }

    audio_block_f32_t *blockOut;
    blockOut = AudioStream_F32::allocate_f32();   // Output block
    if (!blockOut)
        {
        AudioStream_F32::transmit(block);
        AudioStream_F32::release(block);
        return;
        }

    for(int i=0; i<128; i++)
        {
        blockDataIn = block->data[i];

        // Leakage on one coefficient
        coeff[numLeak] *= decay;         // Decay one coefficient
        if(++numLeak >= lengthDataF)     // Wrap around, if needed
            numLeak = 0;

        // Circular delay line to find correlated components
        dataD[kNextD] = blockDataIn;     // Get a new data point from block

#ifdef LMS_NORMALIZE
        powerNorm[i] = blockDataIn*blockDataIn;
        pNorm += powerNorm[i];
        if(i==127)
           pNorm -= powerNorm[0];
        else
           pNorm -= powerNorm[i+1];
#endif

        if(++kNextD >= lengthDataD)     // Next spot in delay line
           kNextD = 0;

        // Update the FIR.
        dataF[kOffsetF] = dataD[kNextD];   // Input FIR is output Delay
        firOut = 0.0f;
        for(j=0; j<lengthDataF; j++)       // Over all coefficients
            {
            k = (j + kOffsetF) & kMask;    // Data circular buffer
            firOut +=coeff[j]*dataF[k];
            }

        // Compute the error, the difference between the data point
        // just received and the FIR output.
        error = blockDataIn - firOut;

        // Update the coefficients
#ifdef LMS_NORMALIZE
        float32_t kcf = error*beta/pNorm;
#else
        float32_t kcf = error*beta;
#endif
        for(j=0; j<lengthDataF; j++)
            {
            k = (j + kOffsetF) & kMask;
            coeff[j] = coeff[j] + kcf*dataF[k];
            }

        // Move to next positions in circular data buffer via kOffsetF
        if(++kOffsetF >= lengthDataF)
            kOffsetF = 0;                   // Wrap the FIR circular buffer

        // fir out to output block
        if(what == DENOISE)
          blockOut->data[i] = firOut;
        else
          blockOut->data[i] = error;           // Auto-Notch
        }
        //transmit the block and be done
        AudioStream_F32::transmit(blockOut);
        AudioStream_F32::release(block);
        AudioStream_F32::release(blockOut);
    }
