/* Extended from Audio Library for Teensy which is
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
 *
 *  Extended by Chip Audette, OpenAudio, Dec 2019
 *  Converted to F32 and to variable audio block length
 *  The F32 conversion is under the MIT License.  Use at your own risk.
 *
 * Further extensions to sub multiple WAV sample rates are copyright
 * (c) 2023 Bob Larkin under the MIT License.
 */

#include <Arduino.h>
#include "AudioSDPlayer_F32.h"
#include "spi_interrupt.h"

#define STATE_DIRECT_8BIT_MONO      0  // playing mono at native sample rate
#define STATE_DIRECT_8BIT_STEREO    1  // playing stereo at native sample rate
#define STATE_DIRECT_16BIT_MONO     2  // playing mono at native sample rate
#define STATE_DIRECT_16BIT_STEREO   3  // playing stereo at native sample rate
#define STATE_CONVERT_8BIT_MONO     4  // playing mono, converting sample rate
#define STATE_CONVERT_8BIT_STEREO   5  // playing stereo, converting sample rate
#define STATE_CONVERT_16BIT_MONO    6  // playing mono, converting sample rate
#define STATE_CONVERT_16BIT_STEREO  7  // playing stereo, converting sample rate
#define STATE_PARSE1            8  // looking for 20 byte ID header
#define STATE_PARSE2            9  // looking for 16 byte format header
#define STATE_PARSE3            10 // looking for 8 byte data header
#define STATE_PARSE4            11 // ignoring unknown chunk after "fmt "
#define STATE_PARSE5            12 // ignoring unknown chunk before "fmt "
#define STATE_PAUSED            13
#define STATE_STOP              14

void AudioSDPlayer_F32::begin(void)
{
    state = STATE_STOP;
    state_play = STATE_STOP;
    data_length = 0;
    if (block_left_f32) {
        AudioStream_F32::release(block_left_f32);
        block_left_f32 = NULL;
    }
    if (block_right_f32) {
        AudioStream_F32::release(block_right_f32);
        block_right_f32 = NULL;
    }
}

bool AudioSDPlayer_F32::play(const char *filename)
{
    stop();
    bool irq = false;
    if (NVIC_IS_ENABLED(IRQ_SOFTWARE)) {
        NVIC_DISABLE_IRQ(IRQ_SOFTWARE);
        irq = true;
    }
#if defined(HAS_KINETIS_SDHC)
    if (!(SIM_SCGC3 & SIM_SCGC3_SDHC)) AudioStartUsingSPI();
#else
    AudioStartUsingSPI();
#endif
    wavfile = SD.open(filename);
    if (!wavfile) {
#if defined(HAS_KINETIS_SDHC)
        if (!(SIM_SCGC3 & SIM_SCGC3_SDHC)) AudioStopUsingSPI();
#else
        AudioStopUsingSPI();
#endif
        if (irq) NVIC_ENABLE_IRQ(IRQ_SOFTWARE);
        return false;
    }
    buffer_length = 0;
    buffer_offset = 0;
    state_play = STATE_STOP;
    data_length = 20;
    header_offset = 0;
    state = STATE_PARSE1;
    if (irq) NVIC_ENABLE_IRQ(IRQ_SOFTWARE);
    return true;
}

void AudioSDPlayer_F32::stop(void)
{
    bool irq = false;
    if (NVIC_IS_ENABLED(IRQ_SOFTWARE)) {
        NVIC_DISABLE_IRQ(IRQ_SOFTWARE);
        irq = true;
    }
    if (state != STATE_STOP) {
        audio_block_f32_t *b1 = block_left_f32;
        block_left_f32 = NULL;
        audio_block_f32_t *b2 = block_right_f32;
        block_right_f32 = NULL;
        state = STATE_STOP;
        if (b1) AudioStream_F32::release(b1);
        if (b2) AudioStream_F32::release(b2);
        wavfile.close();
#if defined(HAS_KINETIS_SDHC)
        if (!(SIM_SCGC3 & SIM_SCGC3_SDHC)) AudioStopUsingSPI();
#else
        AudioStopUsingSPI();
#endif
    }
    if (irq) NVIC_ENABLE_IRQ(IRQ_SOFTWARE);
}

void AudioSDPlayer_F32::togglePlayPause(void) {
    // take no action if wave header is not parsed OR
    // state is explicitly STATE_STOP
    if(state_play >= 8 || state == STATE_STOP) return;

    // toggle back and forth between state_play and STATE_PAUSED
    if(state == state_play) {
        state = STATE_PAUSED;
    }
    else if(state == STATE_PAUSED) {
        state = state_play;
    }
}

void AudioSDPlayer_F32::update(void)
{
    int32_t n;

    // only update if we're playing and not paused
    if (state == STATE_STOP || state == STATE_PAUSED) return;

    // allocate the audio blocks to transmit
    block_left_f32 = AudioStream_F32::allocate_f32();
    if (block_left_f32 == NULL) return;
    if (state < 8 && (state & 1) == 1) {
        // if we're playing stereo, allocate another
        // block for the right channel output
        block_right_f32 = AudioStream_F32::allocate_f32();
        if (block_right_f32 == NULL) {
            AudioStream_F32::release(block_left_f32);
            return;
        }
    } else {
        // if we're playing mono or just parsing
        // the WAV file header, no right-side block
        block_right_f32 = NULL;
    }
    block_offset = 0;

    // is there buffered data?
    n = buffer_length - buffer_offset;
    if (n > 0) {
        // Have buffered data. consume(n) returns true if audio transmitted.
        if (consume(n)) return; // it was enough to transmit audio
    }
    // we only get to this point when buffer[512] is empty
    if (state != STATE_STOP && wavfile.available()) {
        // we can read more data from the file...
 readagain:
        buffer_length = wavfile.read(buffer, 512);
        if (buffer_length == 0) goto end;
        buffer_offset = 0;
        bool parsing = (state >= 8);
        bool txok = consume(buffer_length);
        if (txok) {
            if (state != STATE_STOP) return;
        } else {
            if (state != STATE_STOP) {
                if (parsing && state < 8) goto readagain;
                else goto cleanup;
            }
        }
    }
 end:   // end of file reached or other reason to stop
    wavfile.close();
#if defined(HAS_KINETIS_SDHC)
    if (!(SIM_SCGC3 & SIM_SCGC3_SDHC)) AudioStopUsingSPI();
#else
    AudioStopUsingSPI();
#endif
    state_play = STATE_STOP;
    state = STATE_STOP;
cleanup:
    if (block_left_f32) {
        if (block_offset > 0) {
            for (uint32_t i=block_offset; i < audio_block_samples; i++) {
                block_left_f32->data[i] = 0.0f;
            }
            transmit(block_left_f32, 0);
            if (state < 8 && (state & 1) == 0) {
                transmit(block_left_f32, 1);
            }
        }
        AudioStream_F32::release(block_left_f32);
        block_left_f32 = NULL;
    }
    if (block_right_f32) {
        if (block_offset > 0) {
            for (uint32_t i=block_offset; i < audio_block_samples; i++) {
                block_right_f32->data[i] = 0.0f;
            }
            transmit(block_right_f32, 1);
        }
        AudioStream_F32::release(block_right_f32);
        block_right_f32 = NULL;
    }
}


// Consume already buffered WAV file data.  Returns true if audio transmitted.
bool AudioSDPlayer_F32::consume(uint32_t size)  {
    uint32_t len;
    uint8_t lsb, msb;
    const uint8_t *p;
    int16_t val_int16;
    float32_t rateRatioF;

    rateRatioF = (float32_t) pSampleSubMultiple->rateRatio;
    p = buffer + buffer_offset;
start:
    if (size == 0) return false;
/*
    Serial.print("AudioSDPlayer_F32 consume, ");
    Serial.print("size = ");
    Serial.print(size);
    Serial.print(", buffer_offset = ");
    Serial.print(buffer_offset);
    Serial.print(", data_length = ");
    Serial.print(data_length);
    Serial.print(", space = ");
    Serial.print((audio_block_samples - block_offset) * 2);
    Serial.print(", state = ");
    Serial.println(state);
 */
    switch (state) {
      // parse wav file header, is this really a .wav file?
      case STATE_PARSE1:
        len = data_length;
        if (size < len) len = size;
        memcpy((uint8_t *)header + header_offset, p, len);
        header_offset += len;
        buffer_offset += len;
        data_length -= len;
        if (data_length > 0) return false;
        // parse the header...
        if (header[0] == 0x46464952 && header[2] == 0x45564157)
            {
            //Serial.println("is wav file");
            if (header[3] == 0x20746D66)
                {
                // "fmt " header
                if (header[4] < 16)
                    {
                    // WAV "fmt " info must be at least 16 bytes
                    break;
                    }
                if (header[4] > sizeof(header))
                    {
                    // if such .wav files exist, increasing the
                    // size of header[] should accomodate them...
                    //Serial.println("WAVEFORMATEXTENSIBLE too long");
                    break;
                    }
                //Serial.println("header ok");
                header_offset = 0;
                state = STATE_PARSE2;
                }
            else
                {
                // first chuck is something other than "fmt "
                //Serial.print("skipping \"");
                //Serial.printf("\" (%08X), ", __builtin_bswap32(header[3]));
                //Serial.print(header[4]);
                //Serial.println(" bytes");
                header_offset = 12;
                state = STATE_PARSE5;
                }
            p += len;
            size -= len;
            data_length = header[4];
            goto start;
            }
        //Serial.println("unknown WAV header");
        break;

      // check & extract key audio parameters
      case STATE_PARSE2:
        len = data_length;
        if (size < len) len = size;
        memcpy((uint8_t *)header + header_offset, p, len);
        header_offset += len;
        buffer_offset += len;
        data_length -= len;
        if (data_length > 0) return false;
        if (parse_format())
            {
            //Serial.println("audio format ok");
            p += len;
            size -= len;
            data_length = 8;
            header_offset = 0;
            state = STATE_PARSE3;
            goto start;
            }
        //Serial.println("unknown audio format");
        break;

      // find the data chunk
      case STATE_PARSE3: // 10
        len = data_length;
        if (size < len) len = size;
        memcpy((uint8_t *)header + header_offset, p, len);
        header_offset += len;
        buffer_offset += len;
        data_length -= len;
        if (data_length > 0) return false;

        p += len;
        size -= len;
        data_length = header[1];
        if (header[0] == 0x61746164)
            {
            // TODO: verify offset in file is an even number
            // as required by WAV format.  abort if odd.  Code
            // below will depend upon this and fail if not even.
            leftover_bytes = 0;
            state = state_play;
            if (state & 1)
				{
                // if we're going to start stereo
                // better allocate another output block
                block_right_f32 = AudioStream_F32::allocate_f32();
                if (!block_right_f32) return false;
                }
            total_length = data_length;
            }
        else
            {
            state = STATE_PARSE4;
            }
        goto start;

      // ignore any extra unknown chunks (title & artist info)
      case STATE_PARSE4: // 11
        if (size < data_length)
            {
            data_length -= size;
            buffer_offset += size;
            return false;
            }
        p += data_length;
        size -= data_length;
        buffer_offset += data_length;
        data_length = 8;
        header_offset = 0;
        state = STATE_PARSE3;
        goto start;

      // skip past "junk" data before "fmt " header
      case STATE_PARSE5:
        len = data_length;
        if (size < len) len = size;
        buffer_offset += len;
        data_length -= len;
        if (data_length > 0) return false;
        p += len;
        size -= len;
        data_length = 8;
        state = STATE_PARSE1;
        goto start;

      // playing mono at native sample rate
      case STATE_DIRECT_8BIT_MONO:
        return false;

      // playing stereo at native sample rate
      case STATE_DIRECT_8BIT_STEREO:
        return false;

      // Playing Mono at native sample rate    ****** 16-BIT MONO ******
      case STATE_DIRECT_16BIT_MONO:
        if (size > data_length) // End of WAV file
            size = data_length;
        data_length -= size;
        while (1)
            {
            if(zerosToSend > 0)
                {
                block_left_f32->data[block_offset++] = 0.0f;   // Zeros for interpolation
                zerosToSend--;
                if (block_offset >= audio_block_samples)
                    {
                    if(pSampleSubMultiple->numCoeffs > 1  &&  // i.e., using FIR
                       pSampleSubMultiple->firBufferL )
                        {
                        arm_fir_f32(&fir_instL, block_left_f32->data,
                           block_left_f32->data, block_left_f32->length);
                        }
                    transmit(block_left_f32, 0);  // Mono sends same to L&R
                    transmit(block_left_f32, 1);

                    AudioStream_F32::release(block_left_f32);
                    block_left_f32 = NULL;
                    data_length += size;
                    buffer_offset = p - buffer;
                    if (block_right_f32)
                        AudioStream_F32::release(block_right_f32);
                    if (data_length == 0)
                        state = STATE_STOP;
                    return true;
                    }
                }
            else   // Not zeros, but data
                {
                lsb = *p++;  // Little endian
                msb = *p++;
                size -= 2;   // 2 bytes per word

                // Convert to F32
                val_int16 = (msb << 8) | lsb;
                // Scale up by rateRatioF to account for zeros
                block_left_f32->data[block_offset++] = rateRatioF*((float)val_int16)/(32768.0);

                // For interpolation, each data point is followed by 0.0f's
                zerosToSend = pSampleSubMultiple->rateRatio - 1;  // 0, 1, 3, 7
                if (block_offset >= audio_block_samples)
                    {
					// The FIR update
					if(pSampleSubMultiple->numCoeffs > 1  &&
                           pSampleSubMultiple->firBufferL )
                        {
                        arm_fir_f32(&fir_instL, block_left_f32->data,
                           block_left_f32->data, block_left_f32->length);
                        }
                    transmit(block_left_f32, 0);  // Mono sends same to L&R
                    transmit(block_left_f32, 1);
                    AudioStream_F32::release(block_left_f32);
                    block_left_f32 = NULL;
                    data_length += size;
                    buffer_offset = p - buffer;
                    if (block_right_f32)
                        AudioStream_F32::release(block_right_f32);
                    if (data_length == 0)
                        state = STATE_STOP;
                    return true;
                    }
                }
            }       // End while(1)
            if (size == 0)
                {
                if (data_length == 0) break;
                return false;
                }
            // End of file reached
            if (block_offset > 0)
                {
                // TODO: fill remainder of last block with zero and transmit
                }
            state = STATE_STOP;
            return false;

      // Playing stereo at native sample rate  ****** 16-BIT STEREO ******
      case STATE_DIRECT_16BIT_STEREO:
        if (size > data_length)
            size = data_length;
        data_length -= size;
        if (leftover_bytes)
            {
            block_left_f32->data[block_offset] = header[0];
            //PAH fix problem with left+right channels being swapped
            //RSL Is this actually the CODEC L/R problem?
            leftover_bytes = 0;
            // goto right16;    // RSL What is the deal???
            }
        while (1) {
            if(zerosToSend > 0)
                {
                block_left_f32->data[block_offset]    = 0.0f; // Zeros for interpolation
                block_right_f32->data[block_offset++] = 0.0f;
                zerosToSend--;
                if (block_offset >= audio_block_samples)
                    {
                    if(pSampleSubMultiple->numCoeffs > 1  &&  // i.e., using FIR
                       pSampleSubMultiple->firBufferL)
                        {
                        arm_fir_f32(&fir_instL, block_left_f32->data,
                           block_left_f32->data, block_left_f32->length);
                        arm_fir_f32(&fir_instR, block_right_f32->data,
                           block_right_f32->data, block_right_f32->length);
                        }
                    transmit(block_left_f32,  0);
                    transmit(block_right_f32, 1);
                    AudioStream_F32::release(block_left_f32);
                    block_left_f32 = NULL;
                    data_length += size;
                    buffer_offset = p - buffer;
                    if (block_right_f32)
                        AudioStream_F32::release(block_right_f32);
                    if (data_length == 0)
                        state = STATE_STOP;
                    return true;
                    }
                }
            else   // Not zeros, but data
                {
                lsb = *p++;  // Little endian
                msb = *p++;
                size -= 2;
                if (size == 0)
                    {
                    if (data_length == 0) break;
                    header[0] = (msb << 8) | lsb;
                    leftover_bytes = 2;
                    return false;
                    }
                val_int16 = (int16_t)((msb << 8) | lsb);
                //convert from int16 to float32 spanning +/-1.0
                // Scale up by rateRatioF to account for zeros
                block_left_f32->data[block_offset] = rateRatioF*((float)val_int16)/(32768.0);

 // right16:  See about 15 lines above
                lsb = *p++;
                msb = *p++;
                size -= 2;
                val_int16 = (int16_t)((msb << 8) | lsb);
                // Convert from int16 to float32 spanning +/-1.0
                // Scale up by rateRatioF to account for zeros
                block_right_f32->data[block_offset++] = rateRatioF*((float)val_int16)/(32768.0);
                // For stereo, the number of zeros to send refers to
                // the number of *pairs* of zeros.
                // For interpolation, each data point is followed by 0.0f's
                zerosToSend = pSampleSubMultiple->rateRatio - 1;  // 0, 1, 3, 7
                if (block_offset >= audio_block_samples)
                    {
                    if(pSampleSubMultiple->numCoeffs > 1  &&  // i.e., using FIR
                       pSampleSubMultiple->firBufferL )
                        {
                        arm_fir_f32(&fir_instL, block_left_f32->data,
                           block_left_f32->data, block_left_f32->length);
                        arm_fir_f32(&fir_instR, block_right_f32->data,
                           block_right_f32->data, block_right_f32->length);
                        }
                    transmit(block_left_f32, 0);
                    AudioStream_F32::release(block_left_f32);
                    block_left_f32 = NULL;
                    transmit(block_right_f32, 1);
                    AudioStream_F32::release(block_right_f32);
                    block_right_f32 = NULL;

                    data_length += size;
                    buffer_offset = p - buffer;
                    if (data_length == 0) state = STATE_STOP;
                    return true;
                    }
                if (size == 0)
                    {
                    if (data_length == 0) break;
                    leftover_bytes = 0;
                    return false;
                    }
                }     // Sending data, not zeros
            // end of file reached
            }  // End while(1)
        if (block_offset > 0)
            {
            // TODO: fill remainder of last block with zero and transmit
            }
        state = STATE_STOP;
        return false;

      // playing mono, converting sample rate
      case STATE_CONVERT_8BIT_MONO :
        return false;

      // playing stereo, converting sample rate
      case STATE_CONVERT_8BIT_STEREO:
        return false;

      // playing mono, converting sample rate
      case STATE_CONVERT_16BIT_MONO:
        return false;

      // playing stereo, converting sample rate
      case STATE_CONVERT_16BIT_STEREO:
        return false;

      // ignore any extra data after playing
      // or anything following any error
      case STATE_STOP:
        return false;

      // this is not supposed to happen!
      //default:
        //Serial.println("AudioSDPlayer_F32, unknown state");
    }
    state_play = STATE_STOP;
    state = STATE_STOP;
    return false;
}


bool AudioSDPlayer_F32::parse_format(void) {
    uint8_t num = 0;
    uint16_t format;
    uint16_t channels;
    uint32_t rate, b2m;
    uint16_t bits;

    format = header[0];
    currentWavData.audio_format = header[0];   // uint16_t
    //Serial.print("  format = ");
    //Serial.println(format);
    if (format != 1) return false;

    rate = header[1];
    currentWavData.sample_rate = header[1];    // uint32_t
    // Serial.print("WAV file sample rate = ");  Serial.println(rate);

    // b2m is used to determine playing time.  We base it on the WAV
    // file meta data.  It is allowed to be played at a different rate
    // but all we do is to make the info available via the
    // struct currentWavData  The INO needs to deal with differences.
    // 4294967296000.0 = 2^32 * 1000
    b2m = (uint32_t)((double)4294967296000.0 / (double)rate);

    channels = header[0] >> 16;
    currentWavData.num_channels = header[0] >> 16;   // uint16_t
    //Serial.print("  channels = ");
    //Serial.println(channels);
    if (channels == 1) { }
    else if (channels == 2)
        {
        b2m >>= 1;  // Divide b2m by 2
        num |= 1;
        }
    else
        return false;

    bits = header[3] >> 16;
    currentWavData.bits = header[3] >> 16;   // uint16_t
    //Serial.print("  bits = ");
    //Serial.println(bits);
    if (bits == 8) { }
    else if (bits == 16)
       {
       b2m >>= 1;   // Again divide b2m by 2
       num |= 2;
       }
    else {return false;}

    bytes2millis = b2m;   // Transfer to global
    // Serial.print("  bytes2Millis = "); Serial.println(b2m);
    // we're not checking the byte rate and block align fields
    // if they're not the expected values, all we could do is
    // return false.  Do any real wav files have unexpected
    // values in these other fields?
    state_play = num;
    return true;
}

uint32_t AudioSDPlayer_F32::updateBytes2Millis(void) {
  double b2m;

  //account for sample rate
  b2m = ((double)4294967296000.0 / ((double)sample_rate_Hz));
  //account for channels
  b2m = b2m / ((double)channels);
  //account for bits per second
  if (bits == 16)
    b2m = b2m / 2;
  else if (bits == 24)
    b2m = b2m / 3;  //can we handle 24 bits?  I don't think that we can.
  // if 8-bits, fall through
  return bytes2millis = (uint32_t)b2m;
}

bool AudioSDPlayer_F32::isPlaying(void) {
    uint8_t s = *(volatile uint8_t *)&state;
    return (s < 8);
}

bool AudioSDPlayer_F32::isPaused(void) {
    uint8_t s = *(volatile uint8_t *)&state;
    return (s == STATE_PAUSED);
}

bool AudioSDPlayer_F32::isStopped(void) {
    uint8_t s = *(volatile uint8_t *)&state;
    return (s == STATE_STOP);
}

uint32_t AudioSDPlayer_F32::positionMillis(void) {
    uint8_t s = *(volatile uint8_t *)&state;
    if (s >= 8 && s != STATE_PAUSED) return 0;
    uint32_t tlength = *(volatile uint32_t *)&total_length;
    uint32_t dlength = *(volatile uint32_t *)&data_length;
    uint32_t offset = tlength - dlength;
    uint32_t b2m = *(volatile uint32_t *)&bytes2millis;
    return ((uint64_t)offset * b2m) >> 32;
}

uint32_t AudioSDPlayer_F32::lengthMillis(void) {
    uint8_t s = *(volatile uint8_t *)&state;
    if (s >= 8 && s != STATE_PAUSED) return 0;
    uint32_t tlength = *(volatile uint32_t *)&total_length;
    uint32_t b2m = *(volatile uint32_t *)&bytes2millis;
    return ((uint64_t)tlength * b2m) >> 32;
}
