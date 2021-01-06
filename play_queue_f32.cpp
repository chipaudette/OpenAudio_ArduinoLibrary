/*
*   AudioRecordQueue_F32
*
*   Created: Chip Audette (OpenAudio), Feb 2017
*       Extended from on Teensy Audio Library
*
*   License: MIT License.  Use at your own risk.
*/

#include "play_queue_f32.h"
#include "utility/dspinst.h"

bool AudioPlayQueue_F32::available(void)
{
    if (userblock) return true;
    userblock = allocate_f32();
    if (userblock) return true;
    return false;
}

/* getBuffer() returns a pointer to the data area of an AudioBlock_32
 * that can be loaded in the .INO.  There is only one of these at a
 * time, and they hold 128 float32_t.  allocate_f32 will hold up
 * a return from getBuffer() if Audio memory is not available.  This will
 * be freed up by update().
 */
float32_t * AudioPlayQueue_F32::getBuffer(void)
{
    if (userblock) return userblock->data;
    while (1) {
        userblock = allocate_f32();
        if (userblock) return userblock->data;
        yield();
    }
}

/* playBuffer() can be called anytime after data is
 * loaded to the data block pointed to by getBuffer).
 * This function then enters the pointer to the queue,
 * waiting to be sent in turn.  If the queue is full,
 * this function waits until a spot in the queue is opened
 * up by update() (called by interrupts).
 */
void AudioPlayQueue_F32::playBuffer(void)
{
    uint32_t h;

    if (!userblock) return;
    h = head + 1;
    if (h >= 32) h = 0;
    while (tail == h) ; // wait until space in the queue
    queue[h] = userblock;
    head = h;
    userblock = NULL;
}

void AudioPlayQueue_F32::update(void)
{
    audio_block_f32_t *block;
    uint32_t t;

    t = tail;
    if (t != head) {   // a data block is available to transmit out
        if (++t >= 32) t = 0;  // tail is advanced by one, circularly
        block = queue[t];      // pointer to next block
        tail = t;
        transmit(block);
        release(block);
    }
}

//assume user already has an audio_block that was NOT allocated by this
//playBuffer.  Here, you hand it your buffer.  This object takes ownership
//of it and puts it into the queue
void AudioPlayQueue_F32::playAudioBlock(audio_block_f32_t *audio_block) {
    uint32_t h;

    if (!audio_block) return;
    h = head + 1;
    if (h >= 32) h = 0;
    while (tail == h) ; // wait until space in the queue
    queue[h] = audio_block;
    audio_block->ref_count++; //take ownership of this block
    head = h;
    //userblock = NULL;
}
