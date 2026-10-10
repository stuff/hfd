; The bootblock: allocates the program's memory and its chip memory (memory.i), loads
; main.bin's first part from the sectors after it, and runs it, a3 = the chip memory.
; Kickstart calls it with a6 = ExecBase and a1 = trackdisk's IOStdReq.
; MAIN_SIZE (main.bin's size) and PART1_SIZE (its first part's), multiples of 512, are
; given on the command line.

	include	"memory.i"

_LVOAllocMem	equ	-198
_LVODoIO	equ	-456
MEMF_CHIP	equ	2
MEMF_CLEAR	equ	$10000
CMD_READ	equ	2
TD_MOTOR	equ	9
IO_COMMAND	equ	28
IO_LENGTH	equ	36
IO_DATA		equ	40
IO_OFFSET	equ	44

	dc.b	'DOS',0
	dc.l	0			; checksum, set by mkadf.py
	dc.l	880			; root block

	move.l	a1,a5
	move.l	#CHIP_SIZE,d0
	move.l	#MEMF_CHIP|MEMF_CLEAR,d1
	jsr	_LVOAllocMem(a6)
	move.l	d0,a3
	move.l	#MAIN_SIZE+PART2_MARGIN+BSS_RESERVE,d0
	move.l	#MEMF_CLEAR,d1		; any memory
	jsr	_LVOAllocMem(a6)
	move.l	d0,a4

	; Kickstart 1.3's trackdisk decodes with the blitter, into chip memory: read into the
	; chip block (where the module goes later), then copy.
	move.l	a5,a1
	move.w	#CMD_READ,IO_COMMAND(a1)
	move.l	a3,a0
	add.l	#C_SONG,a0
	move.l	a0,IO_DATA(a1)
	move.l	#PART1_SIZE,IO_LENGTH(a1)
	move.l	#1024,IO_OFFSET(a1)
	jsr	_LVODoIO(a6)
	move.l	a3,a0
	add.l	#C_SONG,a0
	move.l	a4,a1
	move.l	#PART1_SIZE/4-1,d0
.copy:	move.l	(a0)+,(a1)+
	subq.l	#1,d0
	bpl.s	.copy

	move.l	a5,a1
	move.w	#TD_MOTOR,IO_COMMAND(a1)
	clr.l	IO_LENGTH(a1)		; motor off
	jsr	_LVODoIO(a6)

	jmp	(a4)
