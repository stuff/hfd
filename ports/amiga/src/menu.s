; The menu (SPEC.md 5), as the web port's scenes/menu.ts. Like the intro, it computes
; the next frame into the Copper buffer that isn't shown (show, in intro.s): one Copper
; template per cursor position (tools/mkintro.py), every colour following band 0, the
; menu's fade level.

SONG_COUNT	equ	8
ENIGMA_SONG	equ	3
MENU_ROWS	equ	218		; the menu opens the lower border
ENTRY_Y		equ	37		; entry k: lines ENTRY_Y + k × ENTRY_STEP, ENTRY_H lines
ENTRY_STEP	equ	23
ENTRY_H		equ	20
MENU_FADE	equ	8*3		; a fade: 8 levels, 3 VBLs each

KEY_SPACE	equ	$40
KEY_RETURN	equ	$44
KEY_ENTER	equ	$43		; on the keypad
KEY_UP		equ	$4c
KEY_DOWN	equ	$4d

; From the text pages (their frame d0 showed black): the menu fades in.
menu_enter:
	clr.w	v_menu_t(a5)
	move.w	#-1,v_menu_leave(a5)
	move.b	#MODE_MENU,v_mode(a5)
	bra.s	menu_next

; The menu's VBL: the keys, then the next frame.
menu_frame:
	addq.w	#1,v_vbl(a5)
	move.b	v_key(a5),d0
	clr.b	v_key(a5)
	tst.w	v_menu_leave(a5)
	bpl.s	menu_next
	cmp.w	#MENU_FADE,v_menu_t(a5)	; keys only once the fade-in is over
	blo.s	menu_next
	move.b	v_cursor(a5),d1
	cmp.b	#KEY_UP,d0
	bne.s	.down
	subq.b	#1,d1
	bpl.s	.cursor
	moveq	#0,d1
	bra.s	.cursor
.down:	cmp.b	#KEY_DOWN,d0
	bne.s	.space
	addq.b	#1,d1
	cmp.b	#SONG_COUNT,d1
	blo.s	.cursor
	moveq	#SONG_COUNT-1,d1
.cursor:
	move.b	d1,v_cursor(a5)
	bra.s	menu_next
.space:	cmp.b	#KEY_SPACE,d0
	bne.s	.return
	move.w	v_menu_t(a5),v_menu_leave(a5)	; back to the text pages
	bra.s	menu_next
.return:
	cmp.b	#KEY_RETURN,d0
	beq	loading_enter
	cmp.b	#KEY_ENTER,d0
	beq	loading_enter

menu_next:
	addq.w	#1,v_menu_t(a5)
	move.w	v_menu_t(a5),d0
	move.w	v_menu_leave(a5),d1
	bmi.s	.fadein
	; Fading out, then black: 24 VBLs after SPACE, the text pages' first frame (black)
	; is this one, and they resume where they were.
	sub.w	d1,d0
	cmp.w	#MENU_FADE,d0
	blo.s	.out
	move.b	#MODE_RESUME,v_mode(a5)
.out:	ext.l	d0
	divu	#3,d0
	cmp.w	#7,d0
	bls.s	.level
	moveq	#7,d0
	bra.s	.level
.fadein:
	ext.l	d0
	divu	#3,d0
	moveq	#7,d1
	sub.w	d0,d1
	bpl.s	.in
	moveq	#0,d1
.in:	move.w	d1,d0
.level:	move.b	d0,v_bands(a5)
	moveq	#0,d7
	move.b	v_cursor(a5),d7
	add.b	#SCREEN_MENU,d7
	bra	show

; Draws the entry of song d0 with bitplane 3 (index + 8: pink) and the others without.
menu_set_playing:
	LEACHIP	C_MENU+ENTRY_Y*LINE+120,a0
	moveq	#0,d1			; entry
.entry:	moveq	#ENTRY_H-1,d2
	move.l	a0,a1
.line:	moveq	#20-1,d3
.word:	moveq	#0,d4
	cmp.b	d0,d1
	bne.s	.plane3
	move.w	-120(a1),d4		; bitplanes 0 to 2
	or.w	-80(a1),d4
	or.w	-40(a1),d4
.plane3:
	move.w	d4,(a1)+
	dbf	d3,.word
	lea	LINE-40(a1),a1		; the next line's bitplane 3
	dbf	d2,.line
	lea	ENTRY_STEP*LINE(a0),a0
	addq.b	#1,d1
	cmp.b	#SONG_COUNT,d1
	bne.s	.entry
	rts

;
; After RETURN (SPEC.md 6), as the web port's scenes/loading.ts: the music fades out, the
; menu too, then LOADING <song> while the module is read from the disk, DECRUNCHING
; <song> while it is decompressed, and back to the menu with it playing. The main loop
; does the work (main.s), on the requests in v_request. Times in VBLs from RETURN.
;

MUSIC_FADE	equ	87		; the music fades out over 87 VBLs, then the menu fades out
SCREEN_IN	equ	108		; the loading screen fades in
WORD_SWAP	equ	21		; LOADING fades out, DECRUNCHING fades in, 21 VBLs each
MENU_IN		equ	32		; from the last fade-out to the menu with the new song
WORD_Y		equ	78
NAME_Y		equ	106
	ifd	AUTHENTIC
LOADING_MIN	equ	288		; the real disk took that long (Just Spank It)
DECRUNCHING_MIN	equ	852
	else
LOADING_MIN	equ	SCREEN_IN+21+60	; 60 VBLs on screen, at least
	endif
NEVER		equ	$7fff

; The main loop's work.
REQUEST_NONE	equ	0
REQUEST_COMPOSE	equ	1		; the loading screens
REQUEST_LOAD	equ	2		; read the module, then decompress it
REQUEST_LOADED	equ	3		; (read: LOADING can end)
REQUEST_DONE	equ	4		; (decompressed)

loading_enter:
	move.b	v_cursor(a5),v_song(a5)	; even the song playing: the demo loads it again
	clr.w	v_load_t(a5)
	move.w	#NEVER,v_loading_end(a5)
	move.w	#NEVER,v_decrunching_end(a5)
	move.b	#MODE_LOADING,v_mode(a5)
	move.b	#REQUEST_COMPOSE,v_request(a5)
	bra.s	loading_next

; The loading's VBL.
loading_frame:
	addq.w	#1,v_vbl(a5)
	clr.b	v_key(a5)
loading_next:
	addq.w	#1,v_load_t(a5)
	move.w	v_load_t(a5),d4		; d4: t
	; The music fades out linearly, then stops: the module's room is free.
	cmp.w	#MUSIC_FADE,d4
	bhi.s	.faded
	moveq	#MUSIC_FADE,d0
	sub.w	d4,d0
	lsl.w	#6,d0
	divu	#MUSIC_FADE,d0		; 64 to 0
	and.l	#$ffff,d0
	FARBSR	_mt_mastervol
	cmp.w	#MUSIC_FADE,d4
	bne.s	.faded
	FARBSR	_mt_end
	clr.b	v_music(a5)
	clr.b	v_tick_valid(a5)
	move.b	#REQUEST_LOAD,v_request(a5)
.faded:	; LOADING lasts until the module is read, DECRUNCHING until it is decompressed.
	cmp.w	#NEVER,v_loading_end(a5)
	bne.s	.loaded
	cmp.w	#SCREEN_IN+21,d4
	blo.s	.ends
	cmp.b	#REQUEST_LOADED,v_request(a5)
	blo.s	.ends
	move.w	#LOADING_MIN,d0
	cmp.w	d4,d0
	bhs.s	.loading
	move.w	d4,d0
.loading:
	move.w	d0,v_loading_end(a5)
	add.w	#2*WORD_SWAP,d0
	ifd	AUTHENTIC
	cmp.w	#DECRUNCHING_MIN,d0
	bhs.s	.decrunching
	move.w	#DECRUNCHING_MIN,d0
.decrunching:
	else
	add.w	#60,d0
	endif
	move.w	d0,v_decrunching_end(a5)
.loaded:
	cmp.w	v_decrunching_end(a5),d4
	bne.s	.ends
	cmp.b	#REQUEST_DONE,v_request(a5)
	beq.s	.ends
	addq.w	#1,v_decrunching_end(a5)	; not decompressed yet: DECRUNCHING stays
.ends:	; Back to the menu, with the new song.
	move.w	v_decrunching_end(a5),d0
	add.w	#MENU_IN,d0
	cmp.w	d0,d4
	blo.s	.screen
	move.b	v_song(a5),v_playing(a5)
	move.b	v_song(a5),d0
	bsr	menu_set_playing
	bsr	start_music
	move.b	#MODE_MENU,v_mode(a5)
	move.w	#-1,v_menu_t(a5)	; this frame is the menu's first
	move.w	#-1,v_menu_leave(a5)
	bra	menu_next

.screen:
	cmp.w	#SCREEN_IN,d4
	bhs.s	.loadingscreen
	; The menu, fading out from MUSIC_FADE.
	moveq	#0,d0
	sub.w	#MUSIC_FADE,d4
	bls.s	.menulevel
	move.w	d4,d0			; (d0's high word is 0)
	divu	#3,d0
	cmp.w	#7,d0
	bls.s	.menulevel
	moveq	#7,d0
.menulevel:
	move.b	d0,v_bands(a5)
	moveq	#0,d7
	move.b	v_cursor(a5),d7
	add.b	#SCREEN_MENU,d7
	bra	show

.loadingscreen:
	; Its fade: in from SCREEN_IN, out from the end of DECRUNCHING.
	move.w	d4,d0
	sub.w	v_decrunching_end(a5),d0
	bhs.s	.out
	move.w	d4,d0
	sub.w	#SCREEN_IN,d0
	ext.l	d0
	divu	#3,d0
	moveq	#7,d3
	sub.w	d0,d3
	bpl.s	.level
	moveq	#0,d3
	bra.s	.level
.out:	ext.l	d0
	divu	#3,d0
	move.w	d0,d3
	cmp.w	#7,d3
	bls.s	.level
	moveq	#7,d3
.level:	move.b	d3,v_bands(a5)		; d3: the screen's level
	; The word's own fade: LOADING fades out, then DECRUNCHING fades in.
	moveq	#SCREEN_LOADING,d7
	moveq	#0,d1
	move.w	d4,d0
	sub.w	v_loading_end(a5),d0
	bmi.s	.word
	ext.l	d0
	cmp.w	#WORD_SWAP,d0
	bhs.s	.decrunchingword
	divu	#3,d0
	move.w	d0,d1
	bra.s	.word
.decrunchingword:
	moveq	#SCREEN_DECRUNCHING,d7
	sub.w	#WORD_SWAP,d0
	divu	#3,d0
	moveq	#7,d1
	sub.w	d0,d1
	bpl.s	.word
	moveq	#0,d1
.word:	add.w	d3,d1			; both fades
	cmp.w	#7,d1
	bls.s	.wordlevel
	moveq	#7,d1
.wordlevel:
	move.b	d1,v_bands+1(a5)
	bra	show

; The loading screens (main loop): LOADING in the first, DECRUNCHING in the second, both
; with the song's name in pink below.
loading_compose:
	LEACHIP	C_HFD,a1
	LEAFAR	word_loading,a2
	bsr.s	.compose
	LEACHIP	C_HFD2,a1
	LEAFAR	word_decrunching,a2
.compose:
	move.l	a1,a0
	move.w	#32000/4-1,d0
.clear:	clr.l	(a0)+
	dbf	d0,.clear
	lea	WORD_Y*LINE(a1),a0
	move.w	#20*LINE/4-1,d0
.word:	move.l	(a2)+,(a0)+
	dbf	d0,.word
	; The name: the song's entry in the menu, bitplanes 0 to 2, and 3 = their OR.
	moveq	#0,d0
	move.b	v_song(a5),d0
	mulu	#ENTRY_STEP*LINE,d0
	LEACHIP	C_MENU+ENTRY_Y*LINE,a2
	add.l	d0,a2
	lea	NAME_Y*LINE(a1),a0
	moveq	#ENTRY_H-1,d1
.line:	moveq	#20-1,d2
.name:	move.w	(a2),d0
	or.w	40(a2),d0
	or.w	80(a2),d0
	move.w	(a2),(a0)
	move.w	40(a2),40(a0)
	move.w	80(a2),80(a0)
	move.w	d0,120(a0)
	addq.l	#2,a2
	addq.l	#2,a0
	dbf	d2,.name
	lea	LINE-40(a2),a2
	lea	LINE-40(a0),a0
	dbf	d1,.line
	rts
