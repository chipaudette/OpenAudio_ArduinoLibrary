
#include <AudioStream.h>

.cpu cortex-m4
.syntax unified
.thumb
.text
.align	2

/* void memcpy_tointerleaveLRwLen(short *dst, short *srcL, short *srcR, short len); */
 .global	memcpy_tointerleaveLRwLen
.thumb_func
	memcpy_tointerleaveLRwLen:

	@ r0: dst
	@ r1: srcL
	@ r2: srcR
	@ r3: len

#if AUDIO_BLOCK_SAMPLES > 8
	push	{r4-r11,r14}
	// add r14,r0,#(AUDIO_BLOCK_SAMPLES*2)
	add r14,r0,r3  //add the number of samples (which is 1/4 of the 16-bit words to move)
	add r14,r14,r3  //again (half)
	add r14,r14,r3  //again (3/4)
	add r14,r14,r3  //again (all)
	.align 2
.loopLR:
	//Load 2*4 words
	ldmia r1!, {r5,r7,r9,r11}  //1+4
	ldmia r2!, {r6,r8,r10,r12} //1+4	

	pkhbt r3,r5,r6,LSL #16	//1
	pkhtb r4,r6,r5,ASR #16	//1

	pkhbt r5,r7,r8,LSL #16	//1
	pkhtb r6,r8,r7,ASR #16	//1

	pkhbt r7,r9,r10,LSL #16	//1
	pkhtb r8,r10,r9,ASR #16	//1

	pkhbt r9,r11,r12,LSL #16	//1
	pkhtb r10,r12,r11,ASR #16	//1

	//Write 8 Words
	stmia r0!, {r3,r4,r5,r6,r7,r8,r9,r10}	//1+8 -> 5+5+8+9 = 27 Cycles to interleave 32 bytes. 
	
	cmp r14, r0
	bne .loopLR

	pop	{r4-r11,r14}	
#elif AUDIO_BLOCK_SAMPLES == 8
	push	{r4-r8,r14}	

	ldmia r1!, {r5,r7}
	ldmia r2!, {r6,r8}

	pkhbt r3,r5,r6,LSL #16
	pkhtb r4,r6,r5,ASR #16

	pkhbt r5,r7,r8,LSL #16
	pkhtb r6,r8,r7,ASR #16

	stmia r0!, {r3,r4,r5,r6}
	pop	{r4-r8,r14}	
#endif
	BX lr
	