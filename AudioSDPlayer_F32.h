/* ***  AudioSDPlayer_F32.h  ***
 *
 *  Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.comaudio_block_samples
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
 *  Extended by Chip Audette, OpenAudio, Dec 2019
 *  Converted to F32 and to variable audio block length
 *  The F32 conversion is under the MIT License.  Use at your own risk.
*/

/*   WAV File Format
 Bytes    Meaning
1 - 4	“RIFF”	Marks the file as a riff file. Characters are each 1 byte long.
5 - 8	File size (integer)	Size of the overall file - 8 bytes, in bytes
        (32-bit integer).
9 -12	“WAVE”	File Type Header. For our purposes, it always equals “WAVE”.
13-16	“fmt "	Format chunk marker. Includes trailing null
17-20	Length of format data as listed above, e.g., 16
21-22	Type of format (1 is PCM) - 2 byte integer
23-24	Number of Channels - 2 byte integer, e.g. 2
25-28	Sample Rate - 32 byte integer. Common values are 44100 (CD),
        48000 (DAT). Sample Rate = Number of Samples per second, or Hertz.
29-32	176400	(Sample Rate * BitsPerSample * Channels) / 8.
33-34	4	(BitsPerSample * Channels) / 8.1 - 8 bit mono2 - 8 bit
        stereo/16 bit mono4 - 16 bit stereo
35-36	16	Bits per sample or other
37-40	"data"   Marks the beginning of the data section.
41-44	File size (data)	Size of the data section.
*
Sample of WAV file start:
00000000  52494646 66EA6903 57415645 666D7420  RIFFf.i.WAVEfmt
00000010  10000000 01000200 44AC0000 10B10200  ........D.......
00000020  04001000 4C495354 3A000000 494E464F  ....LIST:...INFO
00000030  494E414D 14000000 49205761 6E742054  INAM....I Want T
00000040  6F20436F 6D65204F 76657200 49415254  o Come Over.IART
00000050  12000000 4D656C69 73736120 45746865  ....Melissa Ethe
00000060  72696467 65006461 746100EA 69030100  ridge.data..i...
00000070  FEFF0300 FCFF0400 FDFF0200 0000FEFF  ................
00000080  0300FDFF 0200FFFF 00000100 FEFF0300  ................
00000090  FDFF0300 FDFF0200 FFFF0100 0000FFFF  ................
*/

/* *** SAMPLE RATES ***
 * In the case of WAV files, there is a specified sample rate that is part
 * of the header data.  The file is a stream of numbers.  If these are
 * turned into voltages and played at the specified rate, everything will
 * sound correct.  For shorthand, we will call this the WAV rate.
 *
 * There is also a Teensy Sample Rate for the Audio system, set by things
 * like the hardware clock for I2S.   If this sample rate is the same as
 * the WAV rate, life is simple and we process and output a sample
 * with the WAV rate being achieved.   A second case is for the WAV rate
 * to be an integer sub-multiple of the Teensy Sample Rate.  This would
 * allow the wave file to run with a 12 ksps sample rate and the Teensy
 * Sample Rate to be 48, or 96, ksps.  There is a data interpolator
 * included after the the data has been read from the file.  This requires
 * specification of the sub-multiple integer and a FIR filter to complete
 * the interpolation operation.  The structure sampleSubMultiple,
 * provided by the .INO, communicates this design information from the INO.
 *
 * The third case is to have the Wave rate and the Teensy Sample rate
 * related by a rational fraction.  This would require a rate changet
 * consisting of both a decimator and an interpolator.  None of that is
 * included in this class.
 */

#ifndef AudioSDPlayer_F32_h_
#define AudioSDPlayer_F32_h_

#include "Arduino.h"
#include "AudioSettings_F32.h"
#include "AudioStream_F32.h"

#include <SdFat.h>  //included in Teensy install as of Teensyduino 1.54-bete3

// This communicates the info for running slow WAV file sample rates.
// This one is declared in the .INO
struct subMult {
    uint16_t   rateRatio;   // Should be 1 for no rate change, else 2, 4, 8
    uint16_t   numCoeffs;   // FIR filter
    float32_t* firCoeffs;   // FIR Filter Coeffs
    float32_t* firBufferL;  // pointer to 127 + numCoeffs float32_t, left ch
    float32_t* firBufferR;  // pointer to 127 + numCoeffs float32_t, right ch
    };

// This communicates the important parameters of the WAV file.   This is
// declared in AudioSDPlayer_F32 to provide data to the .INO.
struct wavData {
    uint16_t audio_format;  // Should be 1 for PCM
    uint16_t num_channels;  // 1 for mono, 2 for stereo
    uint32_t sample_rate;   // 44100, 48000, etc
    uint16_t bits;          // Number of bits per sample
    };

class AudioSDPlayer_F32 : public AudioStream_F32
{
//GUI: inputs:0, outputs:2  //this line used for automatic generation of GUI nodes
  public:

    AudioSDPlayer_F32(void) :
            AudioStream_F32(0, NULL), block_left_f32(NULL), block_right_f32(NULL)
        {
	    begin();
	    }

    AudioSDPlayer_F32(const AudioSettings_F32 &settings) :
            AudioStream_F32(0, NULL), block_left_f32(NULL), block_right_f32(NULL)
        {
        setSampleRate_Hz(settings.sample_rate_Hz);
        //setBlockSize(settings.audio_block_samples);  // Always 128
        begin();
        }

    void begin(void);  //begins SD card
    bool play(const char *filename);
    void stop(void);
	void togglePlayPause(void);
	bool isPaused(void);
	bool isStopped(void);
    bool isPlaying(void);
    uint32_t positionMillis(void);
    uint32_t lengthMillis(void);

    // Required when WAV file is at a sub-multiple rate of audio sampling rate
    void setSubMult(subMult* pSampleSubMultipleStruct) {
		if(pSampleSubMultipleStruct->rateRatio == 1 ||
		   pSampleSubMultipleStruct->rateRatio == 2 ||
		   pSampleSubMultipleStruct->rateRatio == 4 ||
		   pSampleSubMultipleStruct->rateRatio == 8)
		       {
		       pSampleSubMultiple = pSampleSubMultipleStruct;
               if(pSampleSubMultiple->numCoeffs > 1  &&
                  pSampleSubMultiple->firBufferL )
                      {
                      arm_fir_init_f32(&fir_instL,
                          pSampleSubMultiple->numCoeffs,
                          (float32_t *)pSampleSubMultiple->firCoeffs,
                          (float32_t *)pSampleSubMultiple->firBufferL,
                          (uint32_t)audio_block_samples);
                      arm_fir_init_f32(&fir_instR,
                          pSampleSubMultiple->numCoeffs,
                          (float32_t *)pSampleSubMultiple->firCoeffs,
                          (float32_t *)pSampleSubMultiple->firBufferR,
                          (uint32_t)audio_block_samples);
				      }
		       }
		else
		   Serial.println("Illegal sub-division multiple for WAV rate.");
	}

    // Provides basic meta-data about WAV file.
	wavData* getCurrentWavData(void) {
		return &currentWavData;      // Pointer to structure
	}

    float32_t setSampleRate_Hz(float32_t fs_Hz) {
        sample_rate_Hz = fs_Hz;
        updateBytes2Millis();
        return sample_rate_Hz;
    }

    virtual void update(void);

  private:
	File wavfile;
	struct subMult* pSampleSubMultiple = &nEqOneTemp;
	// Next is a dummy structure to divide by 1 when no INO structure
	struct subMult nEqOneTemp = {1, 0, NULL, NULL, NULL};
	arm_fir_instance_f32 fir_instL;
	arm_fir_instance_f32 fir_instR;
	struct wavData currentWavData = {1, 2, 44100, 16};
    bool consume(uint32_t size);
    bool parse_format(void);
    uint32_t header[10];    // temporary storage of wav header data
    uint32_t data_length;   // number of bytes remaining in current section
    uint32_t total_length;    // number of audio data bytes in file
    uint16_t channels = 1; //number of audio channels
    uint16_t bits = 16;  // number of bits per sample
    uint32_t bytes2millis;
    // Variables for audio library storage, float32_t
    audio_block_f32_t *block_left_f32 = NULL;
    audio_block_f32_t *block_right_f32 = NULL;
    uint16_t block_offset;    // how much data is in block_left & block_right
    // Variables for buffering the WAV file read, uint8_t
    uint8_t buffer[512];    // buffer one block of SD file data
    uint16_t buffer_offset;   // where we're at consuming "buffer"
    uint16_t buffer_length;   // how many data bytes are in "buffer" (512 until last read)
    uint8_t header_offset;    // number of bytes in header[]
    // Variables to control the WAV file reading
    uint8_t state;
    uint8_t state_play;
    uint8_t leftover_bytes;
    // Variables for WAV file sampled at a sub rate from audio process
    uint8_t zerosToSend = 0;

    static unsigned long update_counter;
    float sample_rate_Hz = ((float)AUDIO_SAMPLE_RATE_EXACT);
    uint16_t audio_block_samples = AUDIO_BLOCK_SAMPLES;

    uint32_t updateBytes2Millis(void);
    //int32_t pctr = 0;
};

#endif
