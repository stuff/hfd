; The intro, from the loading screen (the ear) to the last logo (SPEC.md 3), as the web
; port's scenes/intro.ts. Its clock counts VBLs like the reference recording does (the
; ear appears at VBL 546), so every frame can be checked against rip/out/rec1/video.mkv.
; Build with AUTHENTIC=1 for the real durations of the ear screen; by default, it skips
; whole cycles of LOADING and DECRUNCHING's pulse, as the web port does.

EAR			equ	546
LOADING_TO_DECRUNCHING	equ	973
EAR_OUT			equ	2801
MUSIC			equ	2836
AURA			equ	3169
AURA_OUT		equ	3763
HFD_RULES		equ	3798
HFD_WORDS		equ	3972
HFD_OUT			equ	4560
AEON			equ	4704
AEON_OUT		equ	5256

; The screens, and the Copper buffers' screen before the first frame.
SCREEN_BLACK	equ	0
SCREEN_EAR	equ	1
SCREEN_AURA	equ	2
SCREEN_HFD	equ	3
SCREEN_AEON	equ	4
SCREEN_NONE	equ	-1

; Bytes per line of a screen (4 interleaved bitplanes of 40 bytes), and of a rule strip.
LINE		equ	160
STRIP_ROW	equ	82

intro_init:
	move.w	#EAR-1,v_vbl(a5)
	move.b	#SCREEN_NONE,v_screens(a5)
	move.b	#SCREEN_NONE,v_screens+1(a5)
	; Black until the first frame.
	moveq	#SCREEN_BLACK,d0
	moveq	#0,d1
	bsr	set_screen
	lea	v_copper(a5),a0
	move.l	a0,COP1LC(a6)
	move.w	d0,COPJMP1(a6)
	rts

; Once the second part is loaded.
intro_loaded:
	; The second buffer of HIGH FIDELITY DREAMS starts as a copy of the first.
	LEAFAR	hfd_bitmap,a0
	lea	v_hfd(a5),a1
	move.w	#32000/4-1,d0
.copy:	move.l	(a0)+,(a1)+
	dbf	d0,.copy
	rts

; Computes the next VBL's frame into the Copper buffer that isn't shown.
intro_frame:
	move.w	v_vbl(a5),d0
	addq.w	#1,d0
	ifnd	AUTHENTIC
	; LOADING stays 147 VBLs instead of 427, DECRUNCHING 148 instead of 1828.
	cmp.w	#LOADING_TO_DECRUNCHING-280,d0
	bne.s	.skip1
	add.w	#280,d0
.skip1:	cmp.w	#EAR_OUT-1680,d0
	bne.s	.skip2
	add.w	#1680,d0
.skip2:
	endif
	; The ear stays until the second part is loaded: whole cycles of its pulse more.
	cmp.w	#EAR_OUT,d0
	bne.s	.loaded
	tst.b	v_loaded(a5)
	bne.s	.loaded
	sub.w	#56,d0
.loaded:
	move.w	d0,v_vbl(a5)
	cmp.w	#MUSIC,d0
	bne.s	.music
	bsr	start_music
	move.w	v_vbl(a5),d0
.music:
	; Which screen, and its bands' levels.
	moveq	#SCREEN_EAR,d7
	cmp.w	#EAR,d0
	blo.s	.black
	cmp.w	#EAR_OUT+10,d0
	blo	ear
	moveq	#SCREEN_AURA,d7
	cmp.w	#AURA,d0
	blo.s	.black
	cmp.w	#AURA_OUT+14,d0
	blo	aura
	moveq	#SCREEN_HFD,d7
	cmp.w	#HFD_RULES,d0
	blo.s	.black
	cmp.w	#HFD_OUT+14,d0
	blo	hfd
	moveq	#SCREEN_AEON,d7
	cmp.w	#AEON,d0
	blo.s	.black
	cmp.w	#AEON_OUT+14,d0
	blo	aeon
.black:	moveq	#SCREEN_BLACK,d7
	; falls through

; Makes the Copper buffer that isn't shown show screen d7 with the levels in v_bands,
; and shows it from the next VBL.
show:
	moveq	#1,d1
	sub.b	v_front(a5),d1
	move.l	d7,d0
	bsr	set_screen
	move.b	d1,v_front(a5)
	move.l	a1,COP1LC(a6)
	move.w	v_vbl(a5),v_shown(a5)
	rts

; Copper buffer d1 (0 or 1) shows screen d0 with the levels in v_bands. Returns a1: the
; buffer. Keeps d1.
set_screen:
	lea	v_copper(a5),a1
	tst.b	d1
	beq.s	.first
	lea	COPPER_SIZE(a1),a1
.first:	bsr	screen_data
	cmp.b	v_screens(a5,d1.w),d0
	beq.s	.patch
	; A new screen: its template, and its bitplanes.
	move.b	d0,v_screens(a5,d1.w)
	movem.l	a0-a1,-(sp)
.copy:	move.w	(a0)+,(a1)+
	cmp.l	a0,a3
	bne.s	.copy
	movem.l	(sp)+,a0-a1
	lea	(a1,d2.w),a0		; the first plane pointer's high word
	moveq	#4-1,d3
.plane:	move.l	a4,d4
	swap	d4
	move.w	d4,(a0)
	swap	d4
	move.w	d4,4(a0)
	addq.l	#8,a0
	lea	40(a4),a4
	dbf	d3,.plane
.patch:	; The colours that follow the bands.
	lea	v_bands(a5),a2
	lea	v_overrides(a5),a3
.entry:	move.w	(a6)+,d3		; offset in the list, -1 at the end
	bmi.s	.done
	move.w	(a6)+,d4		; band
	move.b	(a2,d4.w),d5
	bmi.s	.override
	ext.w	d5
	add.w	d5,d5
	move.w	(a6,d5.w),(a1,d3.w)
	lea	16(a6),a6
	bra.s	.entry
.override:
	add.w	d4,d4
	move.w	(a3,d4.w),(a1,d3.w)
	lea	16(a6),a6
	bra.s	.entry
.done:	lea	CUSTOM,a6
	rts

; Screen d0's data for Copper buffer d1: a0 its template, a3 the template's end, a6 its
; patches, a4 its bitmap, d2 the offset of the plane pointers in the list.
screen_data:
	move.w	#EAR_PLANES,d2		; the same in every template
	cmp.b	#SCREEN_EAR,d0
	bne.s	.aura
	lea	ear_copper(pc),a0
	lea	ear_copper_end(pc),a3
	lea	ear_patches(pc),a6
	LEAFAR	ear_bitmap,a4
	rts
.aura:	cmp.b	#SCREEN_AURA,d0
	bne.s	.hfd
	lea	aura_copper(pc),a0
	lea	aura_copper_end(pc),a3
	lea	aura_patches(pc),a6
	LEAFAR	aura_bitmap,a4
	rts
.hfd:	cmp.b	#SCREEN_HFD,d0
	bne.s	.aeon
	lea	hfd_copper(pc),a0
	lea	hfd_copper_end(pc),a3
	lea	hfd_patches(pc),a6
	bsr.s	hfd_buffer
	rts
.aeon:	cmp.b	#SCREEN_AEON,d0
	bne.s	.black
	lea	aeon_copper(pc),a0
	lea	aeon_copper_end(pc),a3
	lea	aeon_patches(pc),a6
	LEAFAR	aeon_bitmap,a4
	rts
.black:	lea	black_copper(pc),a0
	lea	black_copper_end(pc),a3
	lea	black_patches(pc),a6
	LEAFAR	ear_bitmap,a4	; not shown
	rts

; a4: the HIGH FIDELITY DREAMS bitmap of Copper buffer d1.
hfd_buffer:
	LEAFAR	hfd_bitmap,a4
	tst.b	d1
	beq.s	.first
	lea	v_hfd(a5),a4
.first:	rts

start_music:
	LEAFAR	song,a0
	sub.l	a1,a1
	moveq	#0,d0
	bsr	_mt_init
	; ptplayer, like ProTracker, plays the first row on the 6th tick; the demo's replay
	; plays it on the first one.
	lea	mt_data(pc),a0
	move.b	mt_Speed(a0),d0
	subq.b	#1,d0
	move.b	d0,mt_Counter(a0)
	st	v_music(a5)
	rts

;
; The screens: d0 is the VBL. Each one sets its bands' levels, then goes to show.
;

; The ear (SPEC.md 3.1). Band 0: the whole screen, with the loader's fade. Bands 1 and 2:
; colours 14 and 15 on the words' lines, which are LOADING's pulse or the background.
ear:
	; The loader's fade, keeping the STE's low bit (it starts on odd VBLs).
	cmp.w	#EAR_OUT,d0
	bhs.s	.out
	moveq	#6,d1
	cmp.w	#EAR,d0
	bls.s	.level
	move.w	d0,d1
	sub.w	#EAR+1,d1
	lsr.w	#1,d1
	neg.w	d1
	addq.w	#5,d1
	bpl.s	.level
	moveq	#0,d1
	bra.s	.level
.out:	moveq	#7,d1
	cmp.w	#EAR_OUT+10,d0
	bhs.s	.level
	move.w	d0,d1
	sub.w	#EAR_OUT,d1
	lsr.w	#1,d1
	addq.w	#1,d1
.level:	move.b	d1,v_bands(a5)
	; The pulse: a 56-VBL triangle, k = VBL - EAR from 1.
	moveq	#0,d2
	cmp.w	#EAR,d0
	bls.s	.pulse
	move.w	d0,d2
	sub.w	#EAR+1,d2
	divu	#56,d2
	swap	d2
	add.w	d2,d2
	lea	pulse_table(pc),a0
	move.w	(a0,d2.w),d2
.pulse:	; The background, faded.
	lea	ear_background(pc),a0
	add.w	d1,d1
	move.w	(a0,d1.w),d3
	st	v_bands+1(a5)
	st	v_bands+2(a5)
	cmp.w	#LOADING_TO_DECRUNCHING,d0
	bhs.s	.decrunching
	move.w	d3,v_overrides+2(a5)
	move.w	d2,v_overrides+4(a5)
	bra	show
.decrunching:
	move.w	d2,v_overrides+2(a5)
	move.w	d3,v_overrides+4(a5)
	bra	show

; AN AURA PRODUCTION (SPEC.md 3.2): one band per word.
aura:
	move.w	#AURA_OUT,d1
	bsr	fade_out2
	move.w	d2,d3			; the common fade-out
	lea	v_bands(a5),a0
	move.w	#AURA,d1
	moveq	#3-1,d4
.word:	bsr	fade_in2
	bsr	max_level
	add.w	#24,d1
	dbf	d4,.word
	bra	show

; HIGH FIDELITY DREAMS (SPEC.md 3.3). Band 0: the rules, bands 1 to 3: the words.
hfd:
	move.w	#HFD_OUT,d1
	bsr	fade_out2
	move.w	d2,d3			; the common fade-out
	lea	v_bands(a5),a0
	move.w	#HFD_RULES,d1
	bsr	fade_in2
	bsr	max_level
	move.w	#HFD_WORDS,d1
	moveq	#3-1,d4
.word:	bsr	fade_in2
	bsr	max_level
	addq.w	#6,d1
	dbf	d4,.word
	; The rules slide in from the sides, 2 pixels per VBL.
	sub.w	#HFD_RULES,d0
	cmp.w	#142,d0
	bls.s	.slide
	move.w	#142,d0
.slide:	add.w	d0,d0			; 2t
	moveq	#1,d1
	sub.b	v_front(a5),d1
	bsr	hfd_buffer
	LEAFAR	rules,a0		; the top rule: column 2t + 4 of its strip
	lea	14*LINE(a4),a1
	move.w	d0,d1
	addq.w	#4,d1
	bsr	draw_rule
	LEAFAR	rules+8*4*STRIP_ROW,a0 ; the bottom rule: column 284 - 2t
	lea	178*LINE(a4),a1
	move.w	#284,d1
	sub.w	d0,d1
	bsr	draw_rule
	bra	show

; Copies 8 lines of a rule's strip a0, from its column d1, to a1. Keeps d0 and d7.
draw_rule:
	move.w	d1,d2
	lsr.w	#4,d2
	add.w	d2,d2
	add.w	d2,a0			; the first word
	and.w	#15,d1			; the shift
	moveq	#8*4-1,d6
.row:	move.l	a0,a2
	moveq	#20-1,d3
.word:	move.l	(a2),d2
	addq.l	#2,a2
	lsl.l	d1,d2
	swap	d2
	move.w	d2,(a1)+
	dbf	d3,.word
	lea	STRIP_ROW(a0),a0
	dbf	d6,.row
	rts

; CODE AND DESIGN AEON (SPEC.md 3.4): one band per word, each flashing in.
aeon:
	move.w	#AEON_OUT,d1
	bsr	fade_out2
	move.w	d2,d3			; the common fade-out
	lea	v_bands(a5),a0
	move.w	#AEON,d1
	lea	flash_levels(pc),a1
	moveq	#4-1,d4
.word:	moveq	#7,d2			; black before the flash
	move.w	d0,d5
	sub.w	d1,d5
	blo.s	.level
	cmp.w	#FLASH_LENGTH-1,d5
	bls.s	.flash
	moveq	#FLASH_LENGTH-1,d5
.flash:	move.b	(a1,d5.w),d2
.level:	bsr	max_level
	add.w	#12,d1
	dbf	d4,.word
	bra	show

; CODE / AND / DESIGN / AEON each flash in with these levels, one per VBL.
flash_levels:
	dc.b	0,1,2,3,4,5,0,1,2,3,4,5,4,3,2,1,0
FLASH_LENGTH	equ	*-flash_levels
	even

; d2: fade in from level 6, one level every 2 VBLs; 7 (black) before VBL d1. d0 = VBL.
fade_in2:
	moveq	#7,d2
	move.w	d0,d5
	sub.w	d1,d5
	blo.s	.done
	lsr.w	#1,d5
	moveq	#6,d2
	sub.w	d5,d2
	bpl.s	.done
	moveq	#0,d2
.done:	rts

; d2: fade out to black, one level every 2 VBLs from VBL d1 (level 1 there). d0 = VBL.
fade_out2:
	moveq	#0,d2
	move.w	d0,d5
	sub.w	d1,d5
	blo.s	.done
	lsr.w	#1,d5
	addq.w	#1,d5
	moveq	#7,d2
	cmp.w	d2,d5
	bhs.s	.done
	move.w	d5,d2
.done:	rts

; Stores the larger of levels d2 and d3 at (a0)+.
max_level:
	cmp.b	d3,d2
	bhs.s	.store
	move.b	d3,d2
.store:	move.b	d2,(a0)+
	rts

