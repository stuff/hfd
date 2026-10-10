; The bootblock: loads main.bin from the sectors after it into chip memory and runs it.
; Kickstart calls it with a6 = ExecBase and a1 = trackdisk's IOStdReq.
; MAIN_SIZE (main.bin's size, a multiple of 512) is given on the command line.

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
	move.l	#MAIN_SIZE,d0
	move.l	#MEMF_CHIP|MEMF_CLEAR,d1
	jsr	_LVOAllocMem(a6)
	move.l	d0,a4

	move.l	a5,a1
	move.w	#CMD_READ,IO_COMMAND(a1)
	move.l	a4,IO_DATA(a1)
	move.l	#MAIN_SIZE,IO_LENGTH(a1)
	move.l	#1024,IO_OFFSET(a1)
	jsr	_LVODoIO(a6)

	move.l	a5,a1
	move.w	#TD_MOTOR,IO_COMMAND(a1)
	clr.l	IO_LENGTH(a1)		; motor off
	jsr	_LVODoIO(a6)

	jmp	(a4)
