; HFD stage 1: boot sector payload (decrypted, runs at $10)
; recursive disassembly of $000010-$00013a, entries $10
; code bytes: 298 / 298


entry_000010:
    ext.w    d0                          ; 000010: 4880
    move.w   d2, d7                      ; 000012: 3e02
    ext.w    d1                          ; 000014: 4881
    and.w    #$81ff, d1                  ; 000016: c27c81ff
    eor.w    d1, d0                      ; 00001a: b340
    beq.b    L000024                     ; 00001c: 6706
    eori.w   #$7, d7                     ; 00001e: 0a470007
    or.l     d7, d0                      ; 000022: 8087

L000024:
    move.b   d0, -$60(a6)                ; 000024: 1d40ffa0
    eori.w   #$514, d7                   ; 000028: 0a470514
    clr.b    $77a6(a6)                   ; 00002c: 422e77a6
    move.b   d7, $3ac(a6)                ; 000030: 1d4703ac
    addi.w   #$1f8, $7de5(a6)            ; 000034: 066e01f87de5
    move.b   d0, $79a1(a6)               ; 00003a: 1d4079a1
    move.b   #$5, $3aa(a6)               ; 00003e: 1d7c000503aa
    eori.w   #$508, d7                   ; 000044: 0a470508
    clr.b    $77a8(a6)                   ; 000048: 422e77a8
    move.b   d1, $7dc5(a6)               ; 00004c: 1d417dc5
    clr.b    $3a8(a6)                    ; 000050: 422e03a8
    lea.l    (a5, d7.w), a5              ; 000054: 4bf57000
    move.l   #$3ffe0, d3                 ; 000058: 263c0003ffe0
    move.l   $2d0(a5), d4                ; 00005e: 282d02d0

L000062:
    move.l   d4, (a5)+                   ; 000062: 2ac4
    move.l   d4, (a5)+                   ; 000064: 2ac4
    move.l   d4, (a5)+                   ; 000066: 2ac4
    move.l   d4, (a5)+                   ; 000068: 2ac4
    subq.l   #$1, d3                     ; 00006a: 5383
    bne.b    L000062                     ; 00006c: 66f4
    clr.b    $7db0(a6)                   ; 00006e: 422e7db0
    adda.l   #$ffbf8800, a5              ; 000072: dbfcffbf8800
    move.l   $7cce(a5), d5               ; 000078: 2a2d7cce
    move.b   d5, $7207(a5)               ; 00007c: 1b457207
    addi.w   #entry_000010, d5           ; 000080: 06450010
    eori.w   #$80, d5                    ; 000084: 0a450080
    move.b   #$b1, $7920(a5)             ; 000088: 1b7c00b17920
    move.w   d5, -$1fa(a5)               ; 00008e: 3b45fe06
    bchg.b   #$8, d5                     ; 000092: 08450008
    asr.w    #$4, d0                     ; 000096: e840
    move.w   d5, $3a5(a6)                ; 000098: 3d4503a5
    and.w    #$feff, d5                  ; 00009c: ca7cfeff
    move.w   d5, -$1fa(a5)               ; 0000a0: 3b45fe06
    move.w   #$1, -$1fc(a5)              ; 0000a4: 3b7c0001fe04
    movea.l  a4, a3                      ; 0000aa: 264c
    move.w   d4, $7810(a5)               ; 0000ac: 3b447810
    move.l   $7df9(a6), d7               ; 0000b0: 2e2e7df9
    eori.w   #$14, d5                    ; 0000b4: 0a450014
    move.w   d5, $3a5(a6)                ; 0000b8: 3d4503a5
    move.w   $7894(a5), $3a3(a6)         ; 0000bc: 3d6d789403a3
    clr.b    $7827(a5)                   ; 0000c2: 422d7827
    and.w    #$fff8, d5                  ; 0000c6: ca7cfff8
    move.w   $7db9(a6), $7910(a5)        ; 0000ca: 3b6e7db97910
    move.w   d5, $3a5(a6)                ; 0000d0: 3d4503a5
    sub.b    d7, d0                      ; 0000d4: 9007
    move.w   d5, -$1fc(a5)               ; 0000d6: 3b45fe04

L0000da:
    move.b   -$1f5(a5), d5               ; 0000da: 1a2dfe0b
    neg.l    d7                          ; 0000de: 4487
    beq.b    L000130                     ; 0000e0: 674e
    bclr.b   #$0, d0                     ; 0000e2: 08800000
    move.w   $7ddf(a6), d1               ; 0000e6: 322e7ddf
    lsl.w    #$8, d5                     ; 0000ea: e14d
    not.l    d7                          ; 0000ec: 4687
    move.b   -$1f3(a5), -$60(a6)         ; 0000ee: 1d6dfe0dffa0
    move.b   $3ac(a6), d5                ; 0000f4: 1a2e03ac
    sub.w    d0, d5                      ; 0000f8: 9a40
    beq.b    L000102                     ; 0000fa: 6706
    clr.b    $7209(a5)                   ; 0000fc: 422d7209
    beq.b    L0000da                     ; 000100: 67d8

L000102:
    cmpa.l   #$10000, a3                 ; 000102: b7fc00010000
    bge.b    L0000da                     ; 000108: 6cd0
    lsl.w    #$8, d1                     ; 00010a: e149
    lea.l    (a6, d1.w), a6              ; 00010c: 4df61000
    or.w     d3, d7                      ; 000110: 8e43
    lea.l    $7d9f(a6), a6               ; 000112: 4dee7d9f

L000116:
    move.b   (a6), d1                    ; 000116: 1216
    neg.w    d0                          ; 000118: 4440
    beq.b    L000128                     ; 00011a: 670c
    eor.b    d1, (a6)+                   ; 00011c: b31e
    not.w    d0                          ; 00011e: 4640
    bchg.b   d0, d1                      ; 000120: 0141
    or.b     d1, -$101(a6)               ; 000122: 832efeff
    bcc.b    L000116                     ; 000126: 64ee

L000128:
    tst.l    (a4)                        ; 000128: 4a94
    bne.b    L0000da                     ; 00012a: 66ae
    beq.w    $400                        ; 00012c: 670002d2

L000130:
    move.w   #$700, -$5c0(a5)            ; 000130: 3b7c0700fa40
    stop     #$2fff                      ; 000136: 4e722fff
