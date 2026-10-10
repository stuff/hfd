; The trackloader: reads sectors of the disk in df0 without the system, by DMA, and
; decodes them (AmigaDOS MFM: 11 sectors of 512 bytes per track, 2 tracks per cylinder).
; It runs in the main loop while the VBL interrupt shows the screens.

CIAA_PRA	equ	$bfe001
CIAB_PRB	equ	$bfd100
DSKPTH		equ	$020
DSKLEN		equ	$024
DSKSYNC		equ	$07e
ADKCON		equ	$09e
VHPOSR		equ	$006
INTB_DSKBLK	equ	1

; CIA-B port B: the drives' control lines (active low).
PRB_MTR		equ	7
PRB_SEL0	equ	3
PRB_SIDE	equ	2
PRB_DIR		equ	1
PRB_STEP	equ	0
; CIA-A port A: the drives' status (active low).
PRA_RDY		equ	5
PRA_TK0		equ	4

SECTOR		equ	512
TRACK_BYTES	equ	11*SECTOR
MFM_WORDS	equ	6400		; a little more than a revolution
MFM_SECTOR	equ	1088		; a sector from its first sync word: syncs 8 bytes (with
					; the 2 words before), header 56, data 1024

; Reads d1 bytes from disk offset d0 (both multiples of 512) to a0.
load_disk:
	movem.l	d0-d7/a0-a4,-(sp)
	lea	CIAB_PRB,a4
	; Motor on: the motor line is latched when the drive is selected.
	move.b	#$ff,(a4)
	bclr	#PRB_MTR,(a4)
	bclr	#PRB_SEL0,(a4)
.ready:	btst	#PRA_RDY,CIAA_PRA
	bne.s	.ready
	; The heads to cylinder 0.
	bset	#PRB_DIR,(a4)		; outwards
.home:	btst	#PRA_TK0,CIAA_PRA
	beq.s	.homed
	bsr	step
	bra.s	.home
.homed:	moveq	#0,d7			; d7: the heads' cylinder
	bsr	settle
	move.w	#$7f00,ADKCON(a6)
	move.w	#$9500,ADKCON(a6)	; MFM precompensation, word sync, fast (MFM) mode
	move.w	#$4489,DSKSYNC(a6)
	move.w	#$8210,DMACON(a6)	; DMAEN, DSKEN

.track:	move.l	d0,d2
	divu	#TRACK_BYTES,d2		; d2: the track holding offset d0
	move.w	d2,d3
	lsr.w	#1,d3			; its cylinder
	cmp.w	d3,d7
	beq.s	.side
.seek:	bclr	#PRB_DIR,(a4)		; inwards
	addq.w	#1,d7
	cmp.w	d3,d7
	bls.s	.step
	bset	#PRB_DIR,(a4)		; outwards
	subq.w	#2,d7
.step:	bsr	step
	cmp.w	d3,d7
	bne.s	.seek
	bsr	settle
.side:	bset	#PRB_SIDE,(a4)		; the lower head: even tracks
	btst	#0,d2
	beq.s	.read
	bclr	#PRB_SIDE,(a4)
.read:	bsr	read_track
	bsr	decode_track
	bne.s	.read			; a sector is missing: read the track again
	tst.l	d1
	bne.s	.track

	move.w	#$0010,DMACON(a6)	; disk DMA off
	; Motor off.
	bset	#PRB_MTR,(a4)
	bclr	#PRB_SEL0,(a4)
	bset	#PRB_SEL0,(a4)
	movem.l	(sp)+,d0-d7/a0-a4
	rts

; One step of the heads, in the direction already set.
step:
	bclr	#PRB_STEP,(a4)
	nop
	nop
	bset	#PRB_STEP,(a4)
	moveq	#3*16,d4		; 3 ms between steps (64 µs per line)
	bra.s	wait_lines

; The heads settle after a seek: 18 ms.
settle:
	move.w	#18*16,d4
	; falls through

; Waits d4 + 1 raster lines.
wait_lines:
.line:	move.b	VHPOSR(a6),d5
.same:	cmp.b	VHPOSR(a6),d5
	beq.s	.same
	dbf	d4,.line
	rts

; Reads a revolution of the current track into the MFM buffer.
read_track:
	lea	v_mfm(a5),a1
	move.l	a1,DSKPTH(a6)
	move.w	#1<<INTB_DSKBLK,INTREQ(a6)
	move.w	#$8000|MFM_WORDS,DSKLEN(a6)
	move.w	#$8000|MFM_WORDS,DSKLEN(a6)
.wait:	btst	#INTB_DSKBLK,INTREQR+1(a6)
	beq.s	.wait
	move.w	#$4000,DSKLEN(a6)
	move.w	#1<<INTB_DSKBLK,INTREQ(a6)
	rts

; Decodes the sectors of track d2 that the load needs (from disk offset d0, d1 bytes, to
; a0) from the MFM buffer, then advances d0, d1 and a0 past them. Z clear if one is
; missing (a bad checksum): the track must be read again.
decode_track:
	movem.l	d0-d7/a1-a3,-(sp)
	move.l	d0,v_load_from(a5)
	move.l	#$55555555,d6
	moveq	#0,d4
	move.w	d2,d4
	mulu	#TRACK_BYTES,d4		; d4: the track's first byte on the disk
	move.l	d0,d5
	add.l	d1,d5
	move.l	d4,d3
	add.l	#TRACK_BYTES,d3
	cmp.l	d3,d5
	bls.s	.end
	move.l	d3,d5
.end:	move.l	d5,v_load_to(a5)	; where this track's part of the load ends
	; d7: the sectors to find, bits (d0 - d4) / 512 to (d5 - d4) / 512 - 1.
	sub.l	d4,d5
	moveq	#9,d3
	lsr.l	d3,d5
	moveq	#1,d7
	lsl.l	d5,d7
	sub.l	d4,d0
	lsr.l	d3,d0
	moveq	#1,d5
	lsl.l	d0,d5
	sub.l	d5,d7

	lea	v_mfm(a5),a1
.sync:	lea	v_mfm+MFM_WORDS*2-MFM_SECTOR(a5),a3	; the last place a whole sector can start
	cmp.l	a3,a1
	bhs	.scanned
	cmp.w	#$4489,(a1)+
	bne.s	.sync
.syncs:	cmp.w	#$4489,(a1)		; the sector follows its sync words (DMA may have
	bne.s	.sector			; started after the first one)
	addq.l	#2,a1
	bra.s	.syncs
.sector:
	; Its header: $ff, track, sector, sectors before the gap.
	move.l	(a1),d3
	and.l	d6,d3
	add.l	d3,d3
	move.l	4(a1),d5
	and.l	d6,d5
	or.l	d5,d3
	swap	d3
	cmp.b	d2,d3
	bne.s	.sync			; another track
	swap	d3
	lsr.w	#8,d3
	and.w	#15,d3			; d3: the sector
	btst	d3,d7
	beq.s	.sync			; not needed
	; The data's checksum: the XOR of its encoded longs.
	lea	56(a1),a2
	moveq	#0,d5
	move.w	#256-1,d0
.sum:	move.l	(a2)+,d1
	eor.l	d1,d5
	dbf	d0,.sum
	and.l	d6,d5
	move.l	48(a1),d0
	and.l	d6,d0
	add.l	d0,d0
	move.l	52(a1),d1
	and.l	d6,d1
	or.l	d1,d0
	cmp.l	d0,d5
	bne.s	.sync			; a bad sector: it stays in d7
	; Decode it to its place: a0 + (d4 + sector * 512 - first offset).
	moveq	#0,d0
	move.w	d3,d0
	moveq	#9,d1
	lsl.l	d1,d0
	add.l	d4,d0
	sub.l	v_load_from(a5),d0
	lea	(a0,d0.l),a2
	lea	56(a1),a3		; the odd bits, the even ones 512 bytes further
	moveq	#128-1,d0
.decode:
	move.l	(a3)+,d1
	and.l	d6,d1
	add.l	d1,d1
	move.l	508(a3),d5
	and.l	d6,d5
	or.l	d5,d1
	move.l	d1,(a2)+
	dbf	d0,.decode
	lea	MFM_SECTOR-8(a1),a1	; past the data (MFM_SECTOR counts from the first sync)
	bclr	d3,d7
	tst.l	d7
	bne	.sync
.scanned:
	tst.l	d7
	bne.s	.missing
	; Done with this track: advance d0, d1 (on the stack) and a0.
	move.l	v_load_to(a5),d0
	sub.l	v_load_from(a5),d0
	add.l	d0,a0
	sub.l	d0,4(sp)
	move.l	v_load_to(a5),(sp)
	moveq	#0,d0			; Z set
.missing:
	movem.l	(sp)+,d0-d7/a1-a3	; (keeps the flags)
	rts
