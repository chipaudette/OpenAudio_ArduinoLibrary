/*      synth_UniformWhiteNoise_F32.cpp

   License: MIT License.  Use at your own risk.
*/

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

#include "synth_UniformWhiteNoise_F32.h"
#define I32_TO_F32_NORM_FACTOR (2.3283064E-10)
#define Q31_TO_F32_NORM_FACTOR (4.6566129E-10)

void AudioSynthUniformWhiteNoise_F32::update(void)
   {
   audio_block_f32_t *block_f32;
   float32_t *pf;

#if !defined(__ARM_ARCH_7EM__)
   return]
#else
   if (level < 0.000001f)
      return;     // special case, turn off update() if level==0.

   block_f32 = AudioStream_F32::allocate_f32();
   if (!block_f32)
      return;
   pf = block_f32->data;
   for(int i=0;  i<block_f32->length;  i++)
      {
      uint64_t x = rnState;
      x ^= x >> 12;
      x ^= x << 25;
      x ^= x >> 27;
      rnState = x;
      *pf++ = level*((Q31_TO_F32_NORM_FACTOR * (float32_t)((x * UINT64_C(2685821657736338717)) >> 32)) - 1.0f);
      }

   AudioStream_F32::transmit(block_f32);
   AudioStream_F32::release(block_f32);
#endif
   }


