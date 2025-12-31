/*
 * AudioEffectDelay_OA_F32.h
 *
 * Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 * Extended by Chip Audette, Open Audio, Apr 2018
 * Isolated names for Open Audio (F32)  RSL June 2020
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

#ifndef AudioEffecDelay_OA_F32_h_
#define AudioEffecDelay_OA_F32_h_

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "utility/dspinst.h"

#define AUDIO_BLOCK_SIZE_F32 AUDIO_BLOCK_SAMPLES    //what is the maximum length of the F32 audio blocks

// Are these too big???   I think the same as I16---half as much?  <<<<<<<<<<<<<<<<
#if defined(__IMXRT1062__)
  // 4.00 second maximum on Teensy 4.0
  #define DELAY_QUEUE_SIZE_OA  (176512 / AUDIO_BLOCK_SAMPLES)
#elif defined(__MK66FX1M0__)
  // 2.41 second maximum on Teensy 3.6
  #define DELAY_QUEUE_SIZE_OA  (106496 / AUDIO_BLOCK_SIZE_F32)
#elif defined(__MK64FX512__)
  // 1.67 second maximum on Teensy 3.5
  #define DELAY_QUEUE_SIZE_OA  (73728 / AUDIO_BLOCK_SIZE_F32)
#elif defined(__MK20DX256__)
  // 0.45 second maximum on Teensy 3.1 & 3.2
  #define DELAY_QUEUE_SIZE_OA  (19826 / AUDIO_BLOCK_SIZE_F32)
#else
  // 0.14 second maximum on Teensy 3.0
  #define DELAY_QUEUE_SIZE_OA  (6144 / AUDIO_BLOCK_SIZE_F32)
#endif

template<int channels=8>
class AudioEffectDelay_OA_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:delay
public:
    AudioEffectDelay_OA_F32() : AudioStream_F32(1, inputQueueArray) {
        activemask = 0;
        headindex = 0;
        tailindex = 0;
        maxblocks = 0;
        memset(queue, 0, sizeof(queue));
        setSampleRate_Hz(AUDIO_SAMPLE_RATE_EXACT);
    }
    AudioEffectDelay_OA_F32(const AudioSettings_F32 &settings) :
        AudioStream_F32(1,inputQueueArray) {
            activemask = 0;
            headindex = 0;
            tailindex = 0;
            maxblocks = 0;
            memset(queue, 0, sizeof(queue));
            setSampleRate_Hz(settings.sample_rate_Hz);
    }
    void setSampleRate_Hz(float _fs_Hz) {
        //Serial.print("AudioEffectDelay_OA_F32: setSampleRate_Hz to ");
        //Serial.println(_fs_Hz);
        sampleRate_Hz = _fs_Hz;
        //audio_block_len_samples = block_size;  //this is the actual size that is being used in each audio_block_f32
    }
    void delay(uint8_t channel, float milliseconds) {
        if (channel >= channels) return;
        if (milliseconds < 0.0) milliseconds = 0.0;
        uint32_t n = (milliseconds*(sampleRate_Hz/1000.0))+0.5;
        uint32_t nmax = AUDIO_BLOCK_SIZE_F32 * (DELAY_QUEUE_SIZE_OA-1);
        if (n > nmax) n = nmax;
        uint32_t blks = (n + (AUDIO_BLOCK_SIZE_F32-1)) / AUDIO_BLOCK_SIZE_F32 + 1;
        if (!(activemask & (1<<channel))) {
            // enabling a previously disabled channel
            delay_samps[channel] = n;
            if (blks > maxblocks) maxblocks = blks;
            activemask |= (1<<channel);
        } else {
            if (n > delay_samps[channel]) {
                // new delay is greater than previous setting
                if (blks > maxblocks) maxblocks = blks;
                delay_samps[channel] = n;
            } else {
                // new delay is less than previous setting
                delay_samps[channel] = n;
                recompute_maxblocks();
            }
        }
    }
    void disable(uint8_t channel) {
        if (channel >= channels) return;
        // diable this channel
        activemask &= ~(1<<channel);
        // recompute maxblocks for remaining enabled channels
        recompute_maxblocks();
    }   
    virtual void update(void) {
        //Serial.println("AudioEffectDelay_OA_F32: update()...");
        receiveIncomingData();  //put the in-coming audio data into the queue
        discardUnneededBlocksFromQueue();  //clear out queued data this is no longer needed
        transmitOutgoingData();  //put the queued data into the output
    }

private:
    void recompute_maxblocks(void) {
        uint32_t max=0;
        uint32_t channel = 0;
        do {
            if (activemask & (1<<channel)) {
                uint32_t n = delay_samps[channel];
                n = (n + (AUDIO_BLOCK_SIZE_F32-1)) / AUDIO_BLOCK_SIZE_F32 + 1;
                if (n > max) max = n;
            }
        } while(++channel < channels);
        maxblocks = max;
    }
    uint8_t activemask;   // which output channels are active
    uint16_t headindex;    // head index (incoming) data in queue
    uint16_t tailindex;    // tail index (outgoing) data from queue
    uint16_t maxblocks;    // number of blocks needed in queue
//#if DELAY_QUEUE_SIZE_OA * AUDIO_BLOCK_SAMPLES < 65535
//  uint16_t writeposition;
//  uint16_t delay_samps[channels]; // # of samples to delay for each channel
//#else
    int writeposition;     //position within current head buffer in the queue
    uint32_t delay_samps[channels]; // # of samples to delay for each channel
//#endif
    audio_block_f32_t *queue[DELAY_QUEUE_SIZE_OA];
    audio_block_f32_t *inputQueueArray[1];
    float sampleRate_Hz = AUDIO_SAMPLE_RATE_EXACT; //default.  from AudioStream.h??
    //int audio_block_len_samples = AUDIO_BLOCK_SAMPLES;

    void receiveIncomingData(void)
    {
        //Serial.println("AudioEffectDelay_OA_F32::receiveIncomingData:  starting...");

        //prepare the receiving queue
        uint16_t head = headindex;  //what block to write to
        uint16_t tail = tailindex;  //what block to read from
        if (queue[head] == NULL) {
            //if (!Serial) Serial.println("AudioEffectDelay_OA_F32::receiveIncomingData: Allocating queue[head].");
            queue[head] = allocate_f32();
            if (queue[head] == NULL) {
                //if (!Serial) Serial.println("AudioEffectDelay_OA_F32::receiveIncomingData: Null memory 1.  Returning.");
                return;
            }
        }

        //prepare target memory nto which we'll copy the incoming data into the queue
        int dest_ind = writeposition;  //inclusive
        if (dest_ind >= (queue[head]->full_length)) {
            head++; dest_ind = 0;
            if (head >= DELAY_QUEUE_SIZE_OA) head = 0;
            if (queue[head] != NULL) {
                if (head==tail) {tail++; if (tail >= DELAY_QUEUE_SIZE_OA) tail = 0; }
                AudioStream_F32::release(queue[head]);
                queue[head]=NULL;
            }
        }
        if (queue[head]==NULL) {
            queue[head] = allocate_f32();
            if (queue[head] == NULL) {
                if (!Serial) Serial.println("AudioEffectDelay_OA_F32::receiveIncomingData: Null memory 2.  Returning.");
                return;
            }
        }

        //receive the in-coming audio data block
        audio_block_f32_t *input = receiveReadOnly_f32();
        if (input == NULL) {
            //if (!Serial) Serial.println("AudioEffectDelay_OA_F32::receiveIncomingData: Input data is NULL.  Returning.");
            return;
        }
        int n_copy = input->length;
        last_received_block_id = input->id;

        // Now we'll loop over the individual samples of the in-coming data
        float32_t *dest = queue[head]->data;
        float32_t *source = input->data;
        int end_write = dest_ind + n_copy; //this may go past the end of the destination array
        int end_loop = min(end_write,(int)(queue[head]->full_length));  //limit to the end of the array
        int src_count=0, dest_count=dest_ind;
        for (int i=dest_ind; i<end_loop; i++) dest[dest_count++] = source[src_count++];

        //finish writing taking data from the next queue buffer
        if (src_count < n_copy) { // there's still more input data to copy...but we need to roll-over to a new input queue
            head++; dest_ind = 0;
            if (head >= DELAY_QUEUE_SIZE_OA) head = 0;
            if (queue[head] != NULL) {
                if (head==tail) {tail++; if (tail >= DELAY_QUEUE_SIZE_OA) tail = 0; }
                AudioStream_F32::release(queue[head]);
                queue[head]=NULL;
            }

            if (queue[head]==NULL) {
                queue[head] = allocate_f32();
                if (queue[head] == NULL) {
                    Serial.println("AudioEffectDelay_OA_F32::receiveIncomingData: Null memory 3.  Returning.");
                    AudioStream_F32::release(input);
                    return;
                }
            }
            float32_t *dest = queue[head]->data;
            end_loop = end_write - (queue[head]->full_length);
            dest_count = dest_ind;
            for (int i=dest_ind; i < end_loop; i++) dest[dest_count++]=source[src_count++];
        }

        AudioStream_F32::release(input);
        writeposition = dest_count;
        headindex = head;
        tailindex = tail;
    }

    void discardUnneededBlocksFromQueue(void)
    {
        uint16_t head = headindex;  //what block to write to
        uint16_t tail = tailindex;  //last useful block of data
        uint32_t count;

        // discard unneeded blocks from the queue
        if (head >= tail) {
            count = head - tail;
        } else {
            count = DELAY_QUEUE_SIZE_OA + head - tail;
        }
    /*  if (head>0) {
            Serial.print("AudioEffectDelay_OA_F32::discardUnneededBlocksFromQueue: head, tail, count, maxblocks, DELAY_QUEUE_SIZE_OA: ");
            Serial.print(head); Serial.print(", ");
            Serial.print(tail); Serial.print(", ");
            Serial.print(count); Serial.print(", ");
            Serial.print(maxblocks); Serial.print(", ");
            Serial.print(DELAY_QUEUE_SIZE_OA); Serial.print(", ");
            Serial.println();
        } */
        if (count > maxblocks) {
            count -= maxblocks;
            do {
                if (queue[tail] != NULL) {
                    AudioStream_F32::release(queue[tail]);
                    queue[tail] = NULL;
                }
                if (++tail >= DELAY_QUEUE_SIZE_OA) tail = 0;
            } while (--count > 0);
        }
        tailindex = tail;
    }

    void transmitOutgoingData(void)
    {
        uint16_t head = headindex;  //what block to write to
        //uint16_t tail = tailindex;  //last useful block of data
        audio_block_f32_t *output;
        int channel; //, index, prev, offset;
        //const float32_t *src, *end;
        //float32_t *dst;

        // transmit the delayed outputs using queue data
        for (channel = 0; channel < channels; channel++) {
            if (!(activemask & (1<<channel))) continue;
            output = allocate_f32();
            if (!output) continue;

            //figure out where to start pulling the data samples from
            uint32_t ref_samp_long = (head*AUDIO_BLOCK_SIZE_F32) + writeposition; //note that writepoisition has already been
                                                                                  //incremented by the block length by the
                                                                                  //receiveIncomingData method.  We'll adjust it next
            uint32_t offset_samp = delay_samps[channel]+output->length;
            if (ref_samp_long < offset_samp) { //when (ref_samp_long - offset_samp) goes negative, the uint32_t will fail, so we do this logic check
                ref_samp_long = ref_samp_long + (((uint32_t)(DELAY_QUEUE_SIZE_OA))*((uint32_t)AUDIO_BLOCK_SIZE_F32));
            }
            ref_samp_long = ref_samp_long - offset_samp;
            uint16_t source_queue_ind = (uint16_t)(ref_samp_long / ((uint32_t)AUDIO_BLOCK_SIZE_F32));
            int source_samp = (int)(ref_samp_long - (((uint32_t)source_queue_ind)*((uint32_t)AUDIO_BLOCK_SIZE_F32)));

            //pull the data from the first source data block
            int dest_counter=0;
            int n_output = output->length;
            float32_t *dest = output->data;
            audio_block_f32_t *source_block = queue[source_queue_ind];
            if (source_block == NULL) {
                //fill destination with zeros for this source block
                int Iend = min(source_samp+n_output,AUDIO_BLOCK_SIZE_F32);
                for (int Isource = source_samp; Isource < Iend; Isource++) {
                    dest[dest_counter++]=0.0;
                }
            } else {
                //fill destination with this source block's values
                float32_t *source = source_block->data;
                int Iend = min(source_samp+n_output,AUDIO_BLOCK_SIZE_F32);
                for (int Isource = source_samp; Isource < Iend; Isource++) {
                    dest[dest_counter++]=source[Isource];
                }
            }

            //pull the data from the second source data block, if needed
            if (dest_counter < n_output) {
                //yes, we need to keep filling the output
                int Iend = n_output - dest_counter;  //how many more data points do we need
                source_queue_ind++; source_samp = 0;  //which source block will we draw from (and reset the source sample counter)
                if (source_queue_ind >= DELAY_QUEUE_SIZE_OA) source_queue_ind = 0;  //wrap around on our source black.
                source_block = queue[source_queue_ind];  //get the source block
                if (source_block == NULL) { //does it have data?
                    //no, it doesn't have data.  fill destination with zeros
                    for (int Isource = source_samp; Isource < Iend; Isource++) {
                        dest[dest_counter++] = 0.0;
                    }
                } else {
                    //source block does have data.  use this block's values
                    float32_t *source = source_block->data;
                    for (int Isource = source_samp; Isource < Iend; Isource++) {
                        dest[dest_counter++]=source[Isource];
                    }
                }
            }

            //add the id of the last received audio block
            output->id = last_received_block_id;

            //transmit and release
            AudioStream_F32::transmit(output, channel);
            AudioStream_F32::release(output);
        }
    }

    unsigned long last_received_block_id = 0;
};

#endif
