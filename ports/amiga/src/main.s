; High Fidelity Dreams, Amiga port: proof of concept.
; Shows the ear screen with LOADING pulsing (SPEC.md 3.1) and plays Act of Impulse.
; The bootblock loads it anywhere in chip memory: all the code is PC-relative.

	bra	start
	include	"ptplayer.asm"		; also defines CUSTOM, DMACON, INTENA, INTREQ...

COP1LC		equ	$080
COPJMP1		equ	$088
DIWSTRT		equ	$08e
DIWSTOP		equ	$090
DDFSTRT		equ	$092
DDFSTOP		equ	$094
BPL1PTH		equ	$0e0
BPLCON0		equ	$100
BPLCON1		equ	$102
BPLCON2		equ	$104
BPL1MOD		equ	$108
BPL2MOD		equ	$10a
COLOR00		equ	$180
INTB_VERTB	equ	5

; The ST screen's 200 lines start on this PAL line.
TOP		equ	$34
; LOADING's lines.
WORD_FIRST	equ	189
WORD_END	equ	200

start:
	lea	CUSTOM,a6
	move.w	#$7fff,INTENA(a6)	; the system is gone from here on
	move.w	#$7fff,INTREQ(a6)
	move.w	#$7fff,INTREQ(a6)
	move.w	#$7fff,DMACON(a6)

	; Bitplane pointers and palette into the copper list.
	lea	ear(pc),a0
	lea	cop_planes(pc),a1
	moveq	#4-1,d0
.plane:	move.l	a0,d1
	move.w	d1,6(a1)
	swap	d1
	move.w	d1,2(a1)
	addq.l	#8,a1
	lea	40*200(a0),a0
	dbf	d0,.plane
	lea	ear_palette(pc),a0
	lea	cop_palette(pc),a1
	moveq	#16-1,d0
.color:	move.w	(a0)+,2(a1)
	addq.l	#4,a1
	dbf	d0,.color
	lea	ear_palette(pc),a0
	lea	cop_restore(pc),a1
	move.w	13*2(a0),2(a1)
	move.w	14*2(a0),6(a1)
	move.w	15*2(a0),10(a1)

	lea	copper(pc),a0
	move.l	a0,COP1LC(a6)
	move.w	d0,COPJMP1(a6)
	move.w	#$8380,DMACON(a6)	; DMAEN, BPLEN, COPEN

	; The music, on CIA-B timer interrupts.
	sub.l	a0,a0			; vector base: 0 on a 68000
	moveq	#1,d0			; PAL
	bsr	_mt_install
	lea	song(pc),a0
	sub.l	a1,a1
	moveq	#0,d0
	bsr	_mt_init
	lea	_mt_Enable(pc),a0
	st	(a0)
	move.w	#$e000,INTENA(a6)	; INTEN, EXTER

	; Each VBL, the next colour of the pulse.
	moveq	#0,d2			; k - 1 mod 56
	lea	pulse_table(pc),a2
	lea	cop_pulse(pc),a3
.frame:	btst	#INTB_VERTB,INTREQR+1(a6)
	beq.s	.frame
	move.w	#1<<INTB_VERTB,INTREQ(a6)
	move.w	d2,d0
	add.w	d0,d0
	move.w	(a2,d0.w),2(a3)
	addq.w	#1,d2
	cmp.w	#56,d2
	bne.s	.frame
	moveq	#0,d2
	bra.s	.frame

copper:
	dc.w	DIWSTRT,TOP<<8|$81
	dc.w	DIWSTOP,((TOP+200)&$ff)<<8|$c1
	dc.w	DDFSTRT,$0038
	dc.w	DDFSTOP,$00d0
	dc.w	BPLCON0,$4200		; 4 bitplanes, lowres
	dc.w	BPLCON1,0
	dc.w	BPLCON2,0
	dc.w	BPL1MOD,0
	dc.w	BPL2MOD,0
cop_planes:
	dc.w	BPL1PTH,0,BPL1PTH+2,0
	dc.w	BPL1PTH+4,0,BPL1PTH+6,0
	dc.w	BPL1PTH+8,0,BPL1PTH+10,0
	dc.w	BPL1PTH+12,0,BPL1PTH+14,0
cop_palette:
	rept	16
	dc.w	COLOR00+2*REPTN,0
	endr
	; LOADING's lines: colour 15 pulses, colour 14 (DECRUNCHING) is hidden,
	; colour 13 is the small frame on the right.
	dc.w	(TOP+WORD_FIRST)<<8|$07,$fffe
	dc.w	COLOR00+13*2,$008
	dc.w	COLOR00+14*2,$004
cop_pulse:
	dc.w	COLOR00+15*2,$000
	dc.w	(TOP+WORD_END)<<8|$07,$fffe
cop_restore:
	dc.w	COLOR00+13*2,0
	dc.w	COLOR00+14*2,0
	dc.w	COLOR00+15*2,0
	dc.w	$ffff,$fffe

	include	"data.i"

	cnop	0,2
ear:	incbin	"ear.raw"
song:	incbin	"module.mod"
