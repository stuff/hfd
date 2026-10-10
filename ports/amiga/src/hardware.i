; Custom chip registers (offsets from CUSTOM), besides those ptplayer.asm defines
; (CUSTOM, DMACON, INTENA, INTENAR, INTREQ, INTREQR, the audio and CIA registers).

COP1LC		equ	$080
COPJMP1		equ	$088
BPLCON0		equ	$100
SPR0POS		equ	$140

INTB_VERTB	equ	5
INTF_VERTB	equ	1<<INTB_VERTB
LEVEL3_VECTOR	equ	$6c

; Where the program writes the VBL it shows, for tools/capture (an unused vector).
SHOWN_VBL	equ	$1f0

; lea label(pc),An for labels further than 32 KB away (the program is position independent).
LEAFAR	macro
	lea	.here\@(pc),\2
.here\@:
	add.l	#\1-.here\@,\2
	endm
