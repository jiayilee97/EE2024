 	.syntax unified
 	.cpu cortex-m3
 	.thumb
 	.align 2
 	.global	asm_stats
 	.thumb_func

asm_stats:
@  Write assembly language code here to compute the statistical parameters
	push {r4-r12}
	ldr r8, [r1] @r8 is the number of elements
	mov r4, r0
	mov r6,#0 @r6 stores mean
	mov r7,#0 @r7 is counter
	mov r9, #0 @r9 stores variance
	mov r10, #0b11111111111111111111111110010001 @r10 is maximum
	mov r11, #101 @r11 is minimum
	push {r14}
	bl mean
	mov r4, r0
	mov r7, #0
	bl variance
	str r10,[r3]
	str r11,[r2]
	mov r0,r6
	pop {r14}
	pop {r4-r12}

	bx lr

@	loops
mean:
	ldr r5,[r4],#4
	cmp r5,r11
	it lt
	movlt r11, r5
	cmp r10,r5
	it lt
	movlt r10,r5
	add r6,r5
	add r7,#1
	cmp r7,r8
	blt mean
	sdiv r6,r6,r7
	bx lr

variance:
	ldr r5,[r4],#4
	sub r5, r6
	mul r5, r5, r5
	add r9, r5
	add r7, #1
	cmp r7, r8
	blt variance
	sub r7,#1
	sdiv r9, r9, r7
	str r9, [r1]
	bx lr
