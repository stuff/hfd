; ZX0 decompressor (the format of Einar Saukas' ZX0, version 2), from its reference
; decompressor dzx0.c.
;
; a0 = compressed data, a1 = destination. Keeps the registers.
; Decompresses in place if the compressed data ends at least 3 bytes after the
; decompressed data (zx0's "delta").

; The next bit of the stream in C and X. d2: the bits left, then a 1.
ZX0BIT	macro
	add.b	d2,d2
	bne.s	.\@
	move.b	(a0)+,d2
	addx.b	d2,d2
.\@:
	endm

zx0_decompress:
	movem.l	d0-d4/a0-a2,-(sp)
	moveq	#1,d1			; d1: the last offset
	move.b	#$80,d2
.literals:
	bsr	.gamma
.literal:
	move.b	(a0)+,(a1)+
	subq.l	#1,d0
	bne.s	.literal
	ZX0BIT
	bcs	.new
	bsr	.gamma			; a match from the last offset
	bsr	.copy
	ZX0BIT
	bcc	.literals
.new:	; A match from a new offset: its high part (an inverted gamma), its low part.
	moveq	#1,d0
.high:	ZX0BIT
	bcs.s	.highdone
	add.l	d0,d0
	ZX0BIT
	bcs.s	.high
	addq.l	#1,d0
	bra.s	.high
.highdone:
	cmp.l	#256,d0
	beq.s	.done
	moveq	#0,d3
	move.b	(a0)+,d3
	lsl.l	#7,d0
	move.l	d3,d4
	lsr.b	#1,d4
	sub.l	d4,d0
	move.l	d0,d1
	; Its length: a gamma whose first bit is that byte's low bit, plus 1.
	moveq	#1,d0
	btst	#0,d3
	bne.s	.length
.long:	ZX0BIT
	addx.l	d0,d0
	ZX0BIT
	bcc.s	.long
.length:
	addq.l	#1,d0
	bsr	.copy
	ZX0BIT
	bcs	.new
	bra	.literals
.done:	movem.l	(sp)+,d0-d4/a0-a2
	rts

; d0 = an interlaced Elias gamma.
.gamma:	moveq	#1,d0
.bits:	ZX0BIT
	bcs.s	.gammadone
	ZX0BIT
	addx.l	d0,d0
	bra.s	.bits
.gammadone:
	rts

; Copies d0 bytes from d1 bytes back.
.copy:	move.l	a1,a2
	sub.l	d1,a2
.byte:	move.b	(a2)+,(a1)+
	subq.l	#1,d0
	bne.s	.byte
	rts
