; Shared by boot.s and main.s: the cleared memory the bootblock allocates after main.bin
; (main.s checks that its BSS fits).

BSS_RESERVE	equ	64*1024
