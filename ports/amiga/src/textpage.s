; The text pages (SPEC.md 4), as the web port's scenes/textpage.ts: a port of the 68000
; code that runs them on the ST (rip/extracted/disasm/textpage_4000.s).
;
; Their data and state live in a copy of the ST's RAM from $12000 to $25B00, taken just
; before the text pages start (rip/extracted/textpage): the raster script and records,
; the colour tables, the pre-shifted font and bubble, the bubble's path, the text and the
; 33 letter slots. The code reads and writes it at the original addresses (a4 points to
; the image's $1A000, so that a variable at ST address X is at X-TP_BIAS(a4)); the
; pointers stored in it stay ST addresses, plus v_tp_delta to reach them.
;
; What changes is the display:
; - the two ST screens ($60000 and $68000) are Amiga screens (4 interleaved bitplanes),
;   so the drawing routines compute Amiga addresses;
; - the chain of 12 Timer B interrupts (rasters) becomes a Copper list. It is made in the
;   frame it shows, as on the ST: the VBL interrupt rebuilds its raster part before the
;   Copper reaches it (it waits for line TP_RASTERS first).
;
; Each VBL, like the interrupt at $12A22: rasters ($12A36), spreader ($1E552), analysers
; ($1E250); then the music, whose state the analysers read at the next VBL.

TP_BIAS		equ	$1a000
REC		equ	$12b32		; the raster record in use
REC_END		equ	$12ad4
REC_STEP	equ	$12ac4
SCRIPT		equ	$12adc
SCRIPT_COUNT	equ	$12b50
SCRIPT_FIRST	equ	$12d48		; where the script loops back to
SCRIPT_END	equ	$12f64
COLTAB		equ	$12a54		; the colour table: 8 tables of 38 bytes, black to full
HANDLERS	equ	$12b52		; the 12 Timer B handlers
HANDLER_SIZE	equ	$28
NEXT_SCREEN	equ	$1e554
DRAW_SCREEN	equ	$1e55a
BUBBLE_PTR	equ	$1e73c
TEXT_PTR	equ	$1e884
BUBBLE_X	equ	$1e9bc
BUBBLE_Y	equ	$1e9be
TEXT_X		equ	$1e9c0
TEXT_Y		equ	$1e9c2
PAUSE		equ	$1e9c4
PITCH		equ	$1e950
CLEAR_COUNT	equ	$1e96c
SLOTS		equ	$1e9c8		; 33 letters: glyph.l, dx.l, x.l, dy.l, y.l, counter.w
SLOT_SIZE	equ	22
SLOT_COUNT	equ	33
ANALYSERS	equ	$1e2e4		; 4 × level.w, y.b, height.b
FONT		equ	$1ec9e
BUBBLE		equ	$2189e
PATH_START	equ	$21c9e
PATH_END	equ	$2214e
TEXT_START	equ	$2214e
SCREEN_B	equ	$68000

FADE_STEP	equ	38		; the fade-in: +38 bytes of colour table every 3 VBLs, 7 times
FADE_STEPS	equ	7

; The text pages' Copper list: the header, then from line TP_RASTERS the rasters.
TP_PLANES	equ	9*4+2		; the first plane pointer's value
TP_COLOR0	equ	TP_PLANES+8*4	; colour 0 above the first raster
TP_RASTERS	equ	45
TP_RASTER_LIST	equ	TP_COLOR0+2+4	; after the header and its wait
TP_COPPER_SIZE	equ	512
; What a screen's spreader drew in bitplane 1, to erase: the bubble's address (0: none),
; the end of the letters' list, the letters' addresses (odd: a byte pair).
ERASE_SIZE	equ	(2+SLOT_COUNT)*4

; Screen line y's PAL line.
SCREEN_V	equ	59

BLTCON0		equ	$040
BLTCON1		equ	$042
BLTAFWM		equ	$044
BLTALWM		equ	$046
BLTDPT		equ	$054
BLTSIZE		equ	$058
BLTDMOD		equ	$066
BLTAPT		equ	$050
BLTAMOD		equ	$064
DMACONR		equ	$002
DMAB_BBUSY	equ	14
COLOR00		equ	$180
COP_WAIT_END	equ	$fffe

; Called by the intro when it is over: from the next VBL, the text pages. d0 = the VBL.
textpage_start:
	move.w	d0,v_vbl(a5)
	move.w	d0,v_shown(a5)
	move.b	#MODE_TEXT,v_mode(a5)
	move.w	#-1,v_tp_leave(a5)
	LEAFAR	textpage_ram,a0
	sub.l	#$12000,a0
	move.l	a0,v_tp_delta(a5)
	LEAFAR	ste_colors,a0
	move.l	a0,v_tp_ste(a5)
	; Empty screens.
	bsr	wait_blitter
	move.l	#$01000000,BLTCON0(a6)	; D only: clear
	move.w	#0,BLTDMOD(a6)
	LEACHIP	C_AURA,a0
	move.l	a0,BLTDPT(a6)
	move.w	#400<<6|40,BLTSIZE(a6)	; 32000 bytes
	bsr	wait_blitter
	LEACHIP	C_AEON,a0
	move.l	a0,BLTDPT(a6)
	move.w	#400<<6|40,BLTSIZE(a6)
	; The Copper list: its header, and no rasters yet.
	LEACHIP	C_TP_COPPER,a1
	lea	tp_header(pc),a0
	moveq	#(tp_header_end-tp_header)/2-1,d0
.copy:	move.w	(a0)+,(a1)+
	dbf	d0,.copy
	move.l	#$fffffffe,(a1)
	clr.w	v_tp_carry0(a5)
	clr.w	v_tp_carry1(a5)
	bsr	tp_header_for_next
	LEACHIP	C_TP_COPPER,a0
	move.l	a0,COP1LC(a6)
	rts

tp_header:
	dc.w	$08e,$3b79,$090,$03b9,$092,$0030,$094,$00c8	; as the intro's screens
	dc.w	BPLCON0,$3200,$102,$0088,$104,0,$108,120,$10a,120	; 3 bitplanes
	dc.w	$0e0,0,$0e2,0,$0e4,0,$0e6,0,$0e8,0,$0ea,0,$0ec,0,$0ee,0
	dc.w	COLOR00,0
	dc.w	TP_RASTERS<<8|$07,COP_WAIT_END
tp_header_end:

wait_blitter:
	btst	#DMAB_BBUSY,DMACONR(a6)	; (once for the A1000's Agnus)
.wait:	btst	#DMAB_BBUSY,DMACONR(a6)
	bne.s	.wait
	rts

; One VBL of the text pages, at the start of the frame it shows.
textpage_frame:
	LEAFAR	textpage_ram+TP_BIAS-$12000,a4
	; SPACE, once the fade-in is over: fade out, then the menu.
	move.b	v_key(a5),d0
	clr.b	v_key(a5)
	cmp.b	#KEY_SPACE,d0
	bne.s	.keyed
	tst.w	v_tp_leave(a5)
	bpl.s	.keyed
	cmp.w	#3*FADE_STEPS,v_tp_t(a5)
	bls.s	.keyed
	move.w	v_tp_t(a5),v_tp_leave(a5)
.keyed:	bsr	tp_rasters		; first: the Copper reaches them at line TP_RASTERS
	bsr	tp_spreader
	bsr	tp_analysers
	; The fades: every 3 VBLs, 7 times, in from the start, out from SPACE.
	addq.w	#1,v_tp_t(a5)
	moveq	#0,d0
	move.w	v_tp_t(a5),d0
	move.l	#FADE_STEP,d2
	move.w	v_tp_leave(a5),d1
	bmi.s	.fade
	neg.l	d2
	sub.w	d1,d0			; VBLs since SPACE
	cmp.w	#3*FADE_STEPS+3,d0	; then 3 VBLs of black, and the menu
	bhs.s	.menu
	tst.w	d0
	beq.s	.faded
.fade:	cmp.w	#3*FADE_STEPS,d0
	bhi.s	.faded
	divu	#3,d0
	swap	d0
	tst.w	d0
	bne.s	.faded
	add.l	d2,COLTAB-TP_BIAS(a4)
.faded:	bsr	tp_header_for_next
	addq.w	#1,v_vbl(a5)
	move.w	v_vbl(a5),v_shown(a5)
	rts
.menu:	; This frame is black: the menu's first one. From the next, the menu.
	bsr	tp_header_for_next
	addq.w	#1,v_vbl(a5)
	bra	menu_enter

; From the menu, whose last frame was black: the text pages resume where they were, from
; the next frame.
textpage_resume:
	clr.w	v_tp_t(a5)
	move.w	#-1,v_tp_leave(a5)
	move.b	#MODE_TEXT,v_mode(a5)
	LEACHIP	C_TP_COPPER,a0
	move.l	a0,COP1LC(a6)
	addq.w	#1,v_vbl(a5)
	move.w	v_vbl(a5),v_shown(a5)
	rts

; The header for the next frame: the screen drawn in this VBL, and colour 0 above the
; first raster (where the last one left it).
tp_header_for_next:
	LEACHIP	C_TP_COPPER,a1
	move.w	v_tp_carry0(a5),TP_COLOR0(a1)
	LEAFAR	textpage_ram+TP_BIAS-$12000,a4
	bsr	tp_draw_screen
	lea	TP_PLANES(a1),a1
	moveq	#4-1,d1
.plane:	move.l	a0,d2
	swap	d2
	move.w	d2,(a1)
	swap	d2
	move.w	d2,4(a1)
	addq.l	#8,a1
	lea	40(a0),a0
	dbf	d1,.plane
	rts

; a0: the Amiga screen of the ST screen in DRAW_SCREEN.
tp_draw_screen:
	move.l	DRAW_SCREEN-TP_BIAS(a4),d0
	; falls through

; a0: the Amiga screen of ST screen d0.
tp_screen:
	cmp.l	#SCREEN_B,d0
	beq.s	.b
	LEACHIP	C_AURA,a0
	rts
.b:	LEACHIP	C_AEON,a0
	rts

; d0.w: STE colour word (odd nibble order) to the Amiga's $RGB.
ste_color:
	move.l	a0,-(sp)
	move.l	v_tp_ste(a5),a0
	and.w	#$0fff,d0
	add.w	d0,d0
	move.w	(a0,d0.w),d0
	move.l	(sp)+,a0
	rts

;
; $12A36: the rasters. Sets up the 12 handlers from the current record and the colour
; table, steps through the records and the script, then makes the Copper list's rasters.
;
tp_rasters:
	move.l	v_tp_delta(a5),d7
	move.l	REC-TP_BIAS(a4),a0
	add.l	d7,a0
	addq.l	#8,a0
	move.b	(a0)+,v_tp_first(a5)
	move.l	COLTAB-TP_BIAS(a4),a2
	add.l	d7,a2
	move.w	$24(a2),d0
	bsr	ste_color
	move.w	d0,v_tp_text(a5)
	lea	HANDLERS-TP_BIAS(a4),a1
	moveq	#0,d6			; i
.pair:	tst.w	d6
	beq.s	.first
	move.b	(a0)+,0-HANDLER_SIZE+$15(a1)	; the odd handler before's line count
.first:	moveq	#0,d1
	move.b	(a0)+,d1		; hues: even handler in the high nibble, odd in the low
	move.w	d1,d2
	and.w	#$f0,d2
	lsr.w	#2,d2
	move.l	(a2,d2.w),2(a1)		; colours 0 and 1
	move.w	2(a2,d2.w),10(a1)	; colour 8 = 1
	move.b	(a0)+,$15(a1)		; the even handler's line count
	lea	HANDLER_SIZE(a1),a1
	and.w	#$0f,d1
	lsl.w	#2,d1
	move.l	(a2,d1.w),2(a1)
	move.w	2(a2,d1.w),10(a1)
	lea	HANDLER_SIZE(a1),a1
	addq.w	#1,d6
	cmp.w	#6,d6
	bne.s	.pair

	; The next record; at the end of an entry's records, the script goes on.
	move.l	REC_STEP-TP_BIAS(a4),d0
	add.l	d0,REC-TP_BIAS(a4)
	move.l	REC-TP_BIAS(a4),d0
	cmp.l	REC_END-TP_BIAS(a4),d0
	bne.s	.recorded
	move.l	SCRIPT-TP_BIAS(a4),a0	; (ST address)
	move.w	SCRIPT_COUNT-TP_BIAS(a4),d0
	subq.w	#1,SCRIPT_COUNT-TP_BIAS(a4)
	cmp.w	#1,d0			; subq.w / bhi: the entry has run its count
	bhi.s	.again
	lea	12(a0),a0
	cmp.l	#SCRIPT_END,a0
	blo.s	.next
	move.l	#SCRIPT_FIRST,a0
.next:	move.l	a0,SCRIPT-TP_BIAS(a4)
	add.l	d7,a0
	move.w	8(a0),SCRIPT_COUNT-TP_BIAS(a4)
	move.w	10(a0),d0
	ext.l	d0
	move.l	d0,REC_STEP-TP_BIAS(a4)
	sub.l	d7,a0
.again:	add.l	d7,a0
	move.l	(a0),REC-TP_BIAS(a4)
	move.l	4(a0),REC_END-TP_BIAS(a4)
	move.l	REC_STEP-TP_BIAS(a4),d0
	bpl.s	.recorded
	add.l	d0,REC-TP_BIAS(a4)
.recorded:
	; The analysers' lines, from the new record.
	move.l	REC-TP_BIAS(a4),a0
	add.l	d7,a0
	lea	ANALYSERS+2-TP_BIAS(a4),a1
	moveq	#4-1,d0
.band:	move.w	(a0)+,(a1)
	addq.l	#4,a1
	dbf	d0,.band

	; The Copper list's rasters: the text colour, colour 1 above the first raster, then
	; each handler's colours 0 and 1 on its line. (On the ST, colour 8 = colour 1: the
	; analysers draw in bitplanes 0 and 3. Here they only use bitplane 0, see
	; tp_analysers, and there are 3 bitplanes: 1 is the analysers, 2 and 4 the letters,
	; their mixes the text colour.)
	LEACHIP	C_TP_COPPER+TP_RASTER_LIST,a1
	move.w	v_tp_text(a5),d0
	move.w	#COLOR00+2*2,d1
.text:	move.w	d1,(a1)+
	move.w	d0,(a1)+
	addq.w	#2,d1
	cmp.w	#COLOR00+8*2,d1
	bne.s	.text
	move.w	v_tp_carry1(a5),d5	; d5: colour 1
	move.w	#COLOR00+1*2,(a1)+
	move.w	d5,(a1)+
	move.w	v_tp_carry0(a5),d4	; d4: colour 0
	moveq	#0,d7			; d7: past line 255
	moveq	#0,d3
	move.b	v_tp_first(a5),d3	; d3: the line of the next handler (0: never)
	bne.s	.chain
	move.w	#256,d3
.chain:	lea	HANDLERS-TP_BIAS(a4),a2
	moveq	#12-1,d6
.handler:
	cmp.w	#200,d3
	bhs.s	.end
	move.w	2(a2),d0
	bsr	ste_color
	move.w	d0,d4
	move.w	4(a2),d0
	bsr	ste_color
	move.w	d0,d5
	; Colour 1 after the line before, colour 0 in the horizontal blank.
	move.w	d3,d0
	add.w	#SCREEN_V-1,d0
	move.w	#$de,d1
	bsr	tp_wait
	move.w	#COLOR00+1*2,(a1)+
	move.w	d5,(a1)+
	move.w	d3,d0
	add.w	#SCREEN_V,d0
	move.w	#$12,d1
	bsr	tp_wait
	move.w	#COLOR00,(a1)+
	move.w	d4,(a1)+
	moveq	#0,d0
	move.b	$15(a2),d0		; this handler's line count (0: the last one)
	beq.s	.end
	add.w	d0,d3
	lea	HANDLER_SIZE(a2),a2
	dbf	d6,.handler
.end:	move.l	#$fffffffe,(a1)
	move.w	d4,v_tp_carry0(a5)	; where the next frame starts
	move.w	d5,v_tp_carry1(a5)
	rts

; A Copper wait for line d0, horizontal position d1, at (a1)+. d7: already past line 255.
tp_wait:
	cmp.w	#256,d0
	blo.s	.wait
	tst.b	d7
	bne.s	.wait
	move.l	#$ffdffffe,(a1)+
	st	d7
.wait:	move.b	d0,(a1)+
	or.b	#1,d1
	move.b	d1,(a1)+
	move.w	#COP_WAIT_END,(a1)+
	cmp.w	#255,d0
	bne.s	.done
	cmp.b	#$df,d1
	bne.s	.done
	st	d7
.done:	rts

;
; $1E552: the text spreader. Swaps the screens, redraws the bubble and the letters in
; flight (bitplane 1), lands letters (bitplane 2), clears the page when asked.
;
tp_spreader:
	move.l	NEXT_SCREEN-TP_BIAS(a4),d0
	move.l	DRAW_SCREEN-TP_BIAS(a4),NEXT_SCREEN-TP_BIAS(a4)
	move.l	d0,DRAW_SCREEN-TP_BIAS(a4)
	bsr	tp_draw_screen
	move.l	a0,a3			; a3: the screen drawn

	; Erase the bubble and the letters drawn in this screen 2 VBLs ago (bitplane 1).
	lea	v_tp_erase(a5),a1
	cmp.l	#SCREEN_B,d0
	bne.s	.list
	lea	ERASE_SIZE(a1),a1
.list:	move.l	a1,v_tp_list(a5)
	move.l	(a1),d0
	beq.s	.letters
	move.l	d0,a0
	moveq	#16-1,d0
.unbubble:
	clr.l	(a0)
	lea	160(a0),a0
	dbf	d0,.unbubble
.letters:
	move.l	4(a1),a2		; the list's end
	lea	8(a1),a1
	bra.s	.erased
.unletter:
	move.l	(a1)+,d0
	moveq	#8-1,d1
	bclr	#31,d0
	bne.s	.unwrap
	move.l	d0,a0
	btst	#0,d0
	bne.s	.unodd
.uneven:
	clr.w	(a0)
	lea	160(a0),a0
	dbf	d1,.uneven
	bra.s	.erased
.unodd:	clr.b	(a0)
	clr.b	1(a0)
	lea	160(a0),a0
	dbf	d1,.unodd
	bra.s	.erased
.unwrap:
	move.l	d0,a0
.unwrapped:
	clr.b	(a0)
	clr.b	160-39(a0)
	lea	160(a0),a0
	dbf	d1,.unwrapped
.erased:
	cmp.l	a2,a1
	blo.s	.unletter
	move.l	v_tp_list(a5),a1
	lea	8(a1),a0
	move.l	a0,4(a1)		; an empty list

	; The bubble at its current position (x - 4, y - 4), 2 words of bitplane 1.
	move.w	BUBBLE_X-TP_BIAS(a4),d0
	subq.w	#4,d0
	move.w	BUBBLE_Y-TP_BIAS(a4),d1
	subq.w	#4,d1
	lea	tp_lines(pc),a0
	add.w	d1,d1
	add.w	d1,d1
	move.l	(a0,d1.w),d1
	lea	40(a3,d1.l),a0
	move.w	d0,d1
	lsr.w	#4,d1
	add.w	d1,d1
	add.w	d1,a0
	and.w	#15,d0
	lsl.w	#6,d0
	lea	BUBBLE-TP_BIAS(a4),a1
	add.w	d0,a1
	move.l	v_tp_list(a5),a2
	move.l	a0,(a2)			; to erase 2 VBLs later
	moveq	#16-1,d0
.bubble:
	move.l	(a1)+,(a0)
	lea	160(a0),a0
	dbf	d0,.bubble
	; Its next position on the path.
	move.l	BUBBLE_PTR-TP_BIAS(a4),a0
	move.l	a0,d1
	add.l	v_tp_delta(a5),a0
	moveq	#0,d0
	move.b	(a0)+,d0
	add.w	#$20,d0
	move.w	d0,BUBBLE_X-TP_BIAS(a4)
	moveq	#0,d0
	move.b	(a0),d0
	move.w	d0,BUBBLE_Y-TP_BIAS(a4)
	addq.l	#2,d1
	cmp.l	#PATH_END,d1
	blo.s	.path
	move.l	#PATH_START,d1
.path:	move.l	d1,BUBBLE_PTR-TP_BIAS(a4)

	; The 33 letter slots.
	lea	SLOTS-TP_BIAS(a4),a2
	moveq	#SLOT_COUNT-1,d7
.slot:	move.l	(a2),d0
	bmi	.moved
	; Draw it: in flight on bitplane 1, landed (counter <= 0) on bitplane 2.
	add.l	v_tp_delta(a5),d0
	move.l	d0,a1
	move.w	8(a2),d0		; x
	move.w	d0,d1
	and.w	#7,d1
	lsl.w	#4,d1
	add.w	d1,a1			; the glyph pre-shifted by x & 7
	move.w	16(a2),d1		; y
	lea	tp_lines(pc),a0
	add.w	d1,d1
	add.w	d1,d1
	move.l	(a0,d1.w),d1
	lea	40(a3,d1.l),a0
	lsr.w	#3,d0
	add.w	d0,a0			; its byte in bitplane 1
	move.l	a0,d1
	cmp.w	#39,d0
	bne.s	.record
	bset	#31,d1			; its right half wraps to the next line (see .wrap)
.record:
	move.l	a1,-(sp)		; to erase 2 VBLs later
	move.l	v_tp_list(a5),a1
	addq.l	#4,4(a1)
	move.l	4(a1),a1
	move.l	d1,-4(a1)
	move.l	(sp)+,a1
	tst.w	20(a2)
	bgt.s	.flying
	lea	40(a0),a0		; landed: bitplane 2
.flying:
	moveq	#8-1,d1
	btst	#0,d0
	bne.s	.odd
.even:	move.w	(a1)+,d2
	or.w	d2,(a0)
	lea	160(a0),a0
	dbf	d1,.even
	bra.s	.moved
.odd:	cmp.w	#39,d0
	beq.s	.wrap
.oddrow:
	move.w	(a1)+,d2
	or.b	d2,1(a0)
	lsr.w	#8,d2
	or.b	d2,(a0)
	lea	160(a0),a0
	dbf	d1,.oddrow
	bra.s	.moved
	; The last byte of a line: on the ST, the byte after it (+7 in its group) is the
	; first of the next line, in the same bitplane.
.wrap:	move.w	(a1)+,d2
	or.b	d2,160-39(a0)
	lsr.w	#8,d2
	or.b	d2,(a0)
	lea	160(a0),a0
	dbf	d1,.wrap
.moved:	; Move it, count down, and give the slot the next letter when it is done.
	tst.w	20(a2)
	beq.s	.count
	move.l	4(a2),d0
	add.l	d0,8(a2)
	move.l	12(a2),d0
	add.l	d0,16(a2)
.count:	subq.w	#1,20(a2)
	cmp.w	#$fffe,20(a2)
	bne.s	.next
	move.w	#$40,20(a2)
	subq.l	#1,PAUSE-TP_BIAS(a4)
	bmi.s	.letter
	clr.l	4(a2)			; a pause: the slot stays empty
	clr.l	8(a2)
	clr.l	12(a2)
	clr.l	16(a2)
	move.l	#-1,(a2)
	bra.s	.next
.letter:
	bsr	tp_next_letter
.next:	lea	SLOT_SIZE(a2),a2
	dbf	d7,.slot

	; The page clear: 2 lines of bitplane 2 per VBL, from line 198 - n down.
	move.w	CLEAR_COUNT-TP_BIAS(a4),d0
	bmi.s	.cleared
	subq.w	#1,CLEAR_COUNT-TP_BIAS(a4)
	neg.w	d0
	add.w	#198,d0
	lea	tp_lines(pc),a0
	add.w	d0,d0
	add.w	d0,d0
	move.l	(a0,d0.w),d0
	lea	80(a3,d0.l),a0
	moveq	#40/4-1,d0
.clear:	clr.l	(a0)+
	clr.l	160-4(a0)
	dbf	d0,.clear
.cleared:
	rts

; Slot a2 takes the next letter of the text: it flies from the bubble to its place in
; 64 VBLs.
tp_next_letter:
	move.l	v_tp_delta(a5),d6
	move.l	TEXT_PTR-TP_BIAS(a4),a0
	add.l	d6,a0
.ff:	cmp.b	#$ff,(a0)
	bne.s	.text
	move.l	#TEXT_START,a0		; back to the start of the text
	add.l	d6,a0
	bra.s	.ff
.text:	tst.b	(a0)
	bne.s	.placed
	; A new line: x, y, signed pitch; a negative pitch writes right to left.
	addq.l	#1,a0
	moveq	#0,d0
	move.b	(a0)+,d0
	move.w	d0,TEXT_X-TP_BIAS(a4)
	moveq	#0,d0
	move.b	(a0)+,d0
	move.w	d0,TEXT_Y-TP_BIAS(a4)
	move.b	(a0)+,d0
	ext.w	d0
	bpl.s	.pitch
	move.w	#$142,d1
	sub.w	TEXT_X-TP_BIAS(a4),d1
	add.w	d0,d1
	move.w	d1,TEXT_X-TP_BIAS(a4)
.pitch:	move.w	d0,PITCH-TP_BIAS(a4)
.placed:
	; From the bubble to the letter's place, in 16.16 fixed point: d = (to - from) << 16 >> 6.
	move.w	BUBBLE_Y-TP_BIAS(a4),d0
	move.w	TEXT_Y-TP_BIAS(a4),d1
	bsr	.flight
	move.l	d0,16(a2)
	move.l	d1,12(a2)
	move.w	BUBBLE_X-TP_BIAS(a4),d0
	move.w	TEXT_X-TP_BIAS(a4),d1
	bsr	.flight
	move.l	d0,8(a2)
	move.l	d1,4(a2)
	moveq	#0,d0
	move.b	(a0)+,d0
	cmp.b	#$bd,d0
	bne.s	.notclear
	; Clear the page over n VBLs, and wait.
	move.b	(a0)+,d0
	move.w	d0,CLEAR_COUNT-TP_BIAS(a4)
	lsr.w	#1,d0
	move.l	d0,PAUSE-TP_BIAS(a4)
	move.l	#-1,(a2)
	bra.s	.done
.notclear:
	cmp.b	#9,d0
	bne.s	.letter
	move.b	(a0)+,d0		; a pause of 30 × n
	mulu	#30,d0
	move.l	d0,PAUSE-TP_BIAS(a4)
	move.l	#-1,(a2)
	bra.s	.done
.letter:
	sub.w	#$30,d0
	bmi.s	.space
	cmp.w	#$4f,d0
	bhi.s	.space
	mulu	#128,d0
	add.l	#FONT,d0
	move.l	d0,(a2)
	bra.s	.advance
.space:	move.l	#-1,(a2)
.advance:
	move.w	PITCH-TP_BIAS(a4),d0
	add.w	d0,TEXT_X-TP_BIAS(a4)
.done:	sub.l	d6,a0
	move.l	a0,TEXT_PTR-TP_BIAS(a4)
	rts

; d0 = from, d1 = to (words): d0 = from << 16, d1 = (to - from) << 16 >> 6.
.flight:
	sub.w	d0,d1
	swap	d1
	clr.w	d1
	asr.l	#6,d1
	swap	d0
	clr.w	d0
	rts

;
; $1E250: the channel analysers, from the replay's state at the previous VBL (v_tick).
; Vertical stripes in bitplanes 3 and 0, one band per channel.
;
tp_analysers:
	bsr	tp_draw_screen
	move.l	a0,a3			; a3: the screen drawn
	lea	ANALYSERS-TP_BIAS(a4),a2
	lea	v_tick+3*4(a5),a1	; band k follows channel 3 - k
	moveq	#0,d7			; k
.band:	tst.b	v_tick_valid(a5)
	beq.s	.level
	; A note, an instrument or a set-volume on the row just read; or a volume slide
	; (with tone portamento or vibrato) on the other ticks: back to the volume.
	move.b	1(a1),d0		; the effect
	tst.b	v_tick_row(a5)
	beq.s	.slides
	tst.b	(a1)			; a note or an instrument
	bne.s	.trigger
	cmp.b	#$c,d0
	beq.s	.trigger
	bra.s	.level
.slides:
	cmp.b	#5,d0
	beq.s	.trigger
	cmp.b	#6,d0
	beq.s	.trigger
	cmp.b	#$a,d0
	bne.s	.level
.trigger:
	move.w	2(a1),(a2)
.level:	move.w	(a2),d0
	beq.s	.store
	subq.w	#2,d0
	cmp.w	#2,d0
	bge.s	.store
	moveq	#2,d0
.store:	move.w	d0,(a2)
	subq.w	#1,d0			; d0: level - 1
	moveq	#0,d3
	move.b	3(a2),d3		; the height
	bmi	.nextband
	; The stripe: its edges by bits 1-3, its width by bits 4-5.
	move.w	d0,d1
	lsr.w	#1,d1
	and.w	#7,d1
	lsl.w	#4,d1
	lea	tp_patterns(pc),a0
	movem.l	(a0,d1.w),d2/d4-d6	; d2, d3, d5, d6 of the original: d2, d4, d5, d6
	move.w	d0,d1
	and.w	#$30,d1
	lsr.w	#4,d1			; the variant
	tst.w	d0
	bpl.s	.row
	moveq	#0,d2			; level 0: nothing
	moveq	#0,d6
	moveq	#0,d1
.row:	; The 8 longs of a line.
	lea	v_tp_row(a5),a0
	moveq	#-1,d0
	clr.l	(a0)
	clr.l	4(a0)
	clr.l	8(a0)
	clr.l	12(a0)
	clr.l	16(a0)
	clr.l	20(a0)
	clr.l	24(a0)
	clr.l	28(a0)
	add.w	d1,d1
	add.w	d1,d1
	jmp	.variants(pc,d1.w)
.variants:
	bra.w	.v0
	bra.w	.v1
	bra.w	.v2
.v3:	move.l	d2,(a0)+
	move.l	d4,(a0)+
	move.l	d0,(a0)+
	move.l	d0,(a0)+
	move.l	d0,(a0)+
	move.l	d0,(a0)+
	move.l	d5,(a0)+
	move.l	d6,(a0)+
	bra.s	.draw
.v2:	addq.l	#4,a0
	move.l	d2,(a0)+
	move.l	d4,(a0)+
	move.l	d0,(a0)+
	move.l	d0,(a0)+
	move.l	d5,(a0)+
	move.l	d6,(a0)+
	bra.s	.draw
.v1:	addq.l	#8,a0
	move.l	d2,(a0)+
	move.l	d4,(a0)+
	move.l	d5,(a0)+
	move.l	d6,(a0)+
	bra.s	.draw
.v0:	lea	12(a0),a0
	move.l	d2,(a0)+
	move.l	d6,(a0)+
.draw:	; On line y, the 8 longs from word 2 of bitplane 0. (The ST writes each long's high
	; word to bitplane 3 of word 2 + 2k, its low word to bitplane 0 of word 3 + 2k: the
	; same colour, and each of these words is always written.)
	moveq	#0,d0
	move.b	2(a2),d0
	lea	tp_lines(pc),a0
	add.w	d0,d0
	add.w	d0,d0
	move.l	(a0,d0.w),d0
	lea	4(a3,d0.l),a0
	bsr	wait_blitter		; (the band before may still be copying)
	lea	v_tp_row(a5),a4
	move.l	(a4)+,(a0)
	move.l	(a4)+,4(a0)
	move.l	(a4)+,8(a0)
	move.l	(a4)+,12(a0)
	move.l	(a4)+,16(a0)
	move.l	(a4)+,20(a0)
	move.l	(a4)+,24(a0)
	move.l	(a4)+,28(a0)
	LEAFAR	textpage_ram+TP_BIAS-$12000,a4
	; Lines y + 1 to y + height are the same: the blitter copies line y down.
	tst.w	d3
	beq.s	.nextband
	lsl.w	#6,d3
	or.w	#16,d3			; BLTSIZE: height × 16 words
	move.l	#$09f00000,BLTCON0(a6)	; A to D
	move.l	#$ffffffff,BLTAFWM(a6)
	move.w	#-32,BLTAMOD(a6)	; line y again and again
	move.w	#160-32,BLTDMOD(a6)
	move.l	a0,BLTAPT(a6)
	lea	160(a0),a0
	move.l	a0,BLTDPT(a6)
	move.w	d3,BLTSIZE(a6)
.nextband:
	addq.l	#4,a2
	subq.l	#4,a1
	addq.w	#1,d7
	cmp.w	#4,d7
	bne	.band
	bra	wait_blitter

; Line y's offset in a screen: y × 160.
tp_lines:
	rept	256
	dc.l	REPTN*160
	endr

; The stripes' patterns (d2, d3, d5, d6 at $1E314-$1E3E0), by (level - 1) bits 1-3.
tp_patterns:
	dc.l	$1,$3777ffff,$fffff776,$40000000
	dc.l	$13,$777fffff,$ffffff77,$64000000
	dc.l	$137,$77ffffff,$fffffff7,$76400000
	dc.l	$1377,$7fffffff,$ffffffff,$77640000
	dc.l	$13777,$ffffffff,$ffffffff,$f7764000
	dc.l	$13777f,$ffffffff,$ffffffff,$ff776400
	dc.l	$13777ff,$ffffffff,$ffffffff,$fff77640
	dc.l	$13777fff,$ffffffff,$ffffffff,$ffff7764
