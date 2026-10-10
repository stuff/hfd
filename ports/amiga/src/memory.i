; Shared by boot.s and main.s.
;
; The bootblock allocates two blocks: the program (main.bin, PART2_MARGIN bytes, then
; BSS_RESERVE bytes of cleared memory for its variables), in any memory (the A500's slow memory), and the chip
; memory below, which the program finds in a3. Only what the custom chips read lives in
; chip memory: the screens shown, the module, the Copper lists, the disk's DMA buffer.

BSS_RESERVE	equ	32*1024
PART2_MARGIN	equ	4096			; to decompress the program's second part in place

C_EAR		equ	0			; the ear
C_MENU		equ	C_EAR+32000		; the menu (218 lines)
C_AURA		equ	C_MENU+218*160		; AN AURA PRODUCTION, then text page screen A
C_HFD		equ	C_AURA+32000		; HIGH FIDELITY DREAMS, then the loading screen
C_HFD2		equ	C_HFD+32000		; its second buffer
C_AEON		equ	C_HFD2+32000		; CODE AND DESIGN AEON, then text page screen B
C_COPPER	equ	C_AEON+32000		; the screens' 2 Copper lists
COPPER_RESERVE	equ	2048
C_TP_COPPER	equ	C_COPPER+2*COPPER_RESERVE	; the text pages' Copper list
C_MFM		equ	C_TP_COPPER+512		; a track as read from the disk
C_SONG		equ	C_MFM+12800		; the module playing
SONG_RESERVE	equ	204*1024		; the largest module, decompressed in place
CHIP_SIZE	equ	C_SONG+SONG_RESERVE
