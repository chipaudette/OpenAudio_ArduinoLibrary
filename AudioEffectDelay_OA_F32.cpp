/*
 *
 *
 * Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 * Extended by Chip Audette, Open Audio, April 2018
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

#include <Arduino.h>
#include "AudioEffectDelay_OA_F32.h"

void AudioEffectDelay_OA_F32::update(void)
{
    //Serial.println("AudioEffectDelay_OA_F32: update()...");
    receiveIncomingData();  //put the in-coming audio data into the queue
    discardUnneededBlocksFromQueue();  //clear out queued data this is no longer needed
    transmitOutgoingData();  //put the queued data into the output
    return;
}

void AudioEffectDelay_OA_F32::receiveIncomingData(void) {
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
    return;
}


void AudioEffectDelay_OA_F32::discardUnneededBlocksFromQueue(void) {
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

void AudioEffectDelay_OA_F32::transmitOutgoingData(void) {
    uint16_t head = headindex;  //what block to write to
    //uint16_t tail = tailindex;  //last useful block of data
    audio_block_f32_t *output;
    int channel; //, index, prev, offset;
    //const float32_t *src, *end;
    //float32_t *dst;

    // transmit the delayed outputs using queue data
    for (channel = 0; channel < 8; channel++) {
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


