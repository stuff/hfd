; HFD stage 2 (track 0 sector 8, decrypted at $400)
; recursive disassembly of $000400-$0005a0, entries $400
; code bytes: 410 / 416


entry_000400:
    lea.l    -$800(a6), a5               ; 000400: 4beef800
    lea.l    $7fff(a6), a4               ; 000404: 49ee7fff
    move.w   d5, d4                      ; 000408: 3805
    adda.l   #$f97a00, a4                ; 00040a: d9fc00f97a00
    move.b   $536(a5), d7                ; 000410: 1e2d0536
    ext.w    d1                          ; 000414: 4881
    or.w     $238(a5), d1                ; 000416: 826d0238
    eori.b   #$64, d7                    ; 00041a: 0a070064
    eori.w   #$3fff, d1                  ; 00041e: 0a413fff
    move.l   -$104(a6), d2               ; 000422: 242efefc
    eori.w   #$20, d4                    ; 000426: 0a440020
    or.w     d1, d2                      ; 00042a: 8441
    eori.l   #$252e535f, d2              ; 00042c: 0a82252e535f
    cmp.l    -$ff(a4), d2                ; 000432: b4acff01
    bne.b    L00047a                     ; 000436: 6642
    lea.l    (a6, d1.w), a6              ; 000438: 4df61000
    move.l   a6, d0                      ; 00043c: 200e
    ror.w    #$8, d0                     ; 00043e: e058
    movep.w  d0, -$7cff(a5)              ; 000440: 018d8301
    move.w   $140(a5), d0                ; 000444: 302d0140
    lea.l    -$1160(a6), a4              ; 000448: 49eeeea0

L00044c:
    move.w   (a4)+, $3cd0(a6)            ; 00044c: 3d5c3cd0
    move.w   (a4)+, $3cd8(a6)            ; 000450: 3d5c3cd8
    move.w   (a4)+, $3ce0(a6)            ; 000454: 3d5c3ce0
    move.w   (a4)+, $3ce8(a6)            ; 000458: 3d5c3ce8
    move.w   (a4)+, $3cf0(a6)            ; 00045c: 3d5c3cf0
    move.w   (a4)+, $3cf8(a6)            ; 000460: 3d5c3cf8
    move.w   (a4)+, $3d00(a6)            ; 000464: 3d5c3d00
    move.w   (a4)+, $3d08(a6)            ; 000468: 3d5c3d08
    lea.l    $a0(a6), a6                 ; 00046c: 4dee00a0
    dbra     d0, L00044c                 ; 000470: 51c8ffda
    move.w   d0, -$7cbe(a5)              ; 000474: 3b408342

L000478:
    bvc.b    L000478                     ; 000478: 68fe

L00047a:
    lea.l    -$6f8(a6), a4               ; 00047a: 49eef908
    eori.w   #$23c6, d2                  ; 00047e: 0a4223c6

L000482:
    clr.b    -$78f3(a5)                  ; 000482: 422d870d
    move.b   d7, $5b4(a5)                ; 000486: 1b4705b4
    neg.w    d2                          ; 00048a: 4442
    beq.b    L0004be                     ; 00048c: 6730
    move.b   d7, $112(a5)                ; 00048e: 1b470112
    move.b   d7, -$78f5(a5)              ; 000492: 1b47870b
    eor.b    d4, $63e(a5)                ; 000496: b92d063e
    add.w    (a4), d0                    ; 00049a: d054
    move.w   d0, -$6dc(a6)               ; 00049c: 3d40f924
    eori.w   #$4efc, d5                  ; 0004a0: 0a454efc
    not.w    d2                          ; 0004a4: 4642
    move.b   d5, -$7cff(a5)              ; 0004a6: 1b458301
    clr.b    -$78f7(a5)                  ; 0004aa: 422d8709
    clr.w    (a4)+                       ; 0004ae: 425c
    clr.b    -$6ed(a6)                   ; 0004b0: 422ef913
    eori.w   #$5e02, d7                  ; 0004b4: 0a475e02
    eor.b    d4, -$1ee(a6)               ; 0004b8: b92efe12
    bvc.b    L000482                     ; 0004bc: 68c4

L0004be:
    move.w   d0, d1                      ; 0004be: 3200
    eori.w   #$e40b, d1                  ; 0004c0: 0a41e40b
    add.w    d1, -$6ee(a6)               ; 0004c4: d36ef912
    and.w    (a7)+, d1                   ; 0004c8: c25f
    or.b     -$4f9(a5), d1               ; 0004ca: 822dfb07
    or.w     d0, d1                      ; 0004ce: 8240
    eori.w   #$604f, d0                  ; 0004d0: 0a40604f
    eori.w   #$c9fa, d5                  ; 0004d4: 0a45c9fa
    or.b     -$4f7(a5), d5               ; 0004d8: 8a2dfb09
    swap     d2                          ; 0004dc: 4842
    eori.w   #$fac2, d2                  ; 0004de: 0a42fac2
    eori.w   #$e7db, d1                  ; 0004e2: 0a41e7db
    move.w   d2, (a5, d5.w)              ; 0004e6: 3b825000
    or.w     #$100, d2                   ; 0004ea: 847c0100
    move.w   d2, $2(a5, d0.w)            ; 0004ee: 3b820002
    and.w    (a7)+, d2                   ; 0004f2: c45f
    move.w   d1, (a5, d5.w)              ; 0004f4: 3b815000
    sub.w    #$c, d1                     ; 0004f8: 927c000c
    move.b   d1, -$7cfd(a5)              ; 0004fc: 1b418303
    addq.w   #$1, d2                     ; 000500: 5242
    move.l   -$2d2(a6), d6               ; 000502: 2c2efd2e
    move.w   d2, (a5, d0.w)              ; 000506: 3b820000
    move.w   d2, -$6dc(a6)               ; 00050a: 3d42f924
    move.w   d1, (a5, d5.w)              ; 00050e: 3b815000
    move.b   $5cc(a5), (a5, d0.w)        ; 000512: 1bad05cc0000
    subq.w   #$4, d1                     ; 000518: 5941
    eori.w   #$8784, d0                  ; 00051a: 0a408784
    move.b   d0, -$6f0(a6)               ; 00051e: 1d40f910
    move.w   d0, (a5, d5.w)              ; 000522: 3b805000
    move.w   d1, -$2(a5, d5.w)           ; 000526: 3b8150fe

L00052a:
    move.b   -$4ff(a5), d0               ; 00052a: 102dfb01
    and.b    d4, d0                      ; 00052e: c004
    beq.b    L00053e                     ; 000530: 670c
    move.b   d0, -$7cff(a5)              ; 000532: 1b408301
    neg.l    d6                          ; 000536: 4486
    beq.b    L00053e                     ; 000538: 6704
    not.l    d6                          ; 00053a: 4686
    bvc.b    L00052a                     ; 00053c: 68ec

L00053e:
    movea.w  d0, a0                      ; 00053e: 3040
    bsr.w    sub_000546                  ; 000540: 61000004
    exg.l    d1, d0                      ; 000544: c340

sub_000546:
    move.w   #$7fff, sr                  ; 000546: 46fc7fff
    move.l   (a7)+, d0                   ; 00054a: 201f
    move.l   d0, d1                      ; 00054c: 2200
    swap     d1                          ; 00054e: 4841
    or.w     d0, d1                      ; 000550: 8240
    move.w   (a7)+, d0                   ; 000552: 301f
    eor.w    d1, d0                      ; 000554: b340
    move.w   $2(a7), d1                  ; 000556: 322f0002
    eori.w   #$73e, d1                   ; 00055a: 0a41073e
    movea.w  d1, a7                      ; 00055e: 3e41
    or.w     $66a(a5), d4                ; 000560: 886d066a
    eor.w    d4, d0                      ; 000564: b940
    sub.w    #$8607, d5                  ; 000566: 9a7c8607

L00056a:
    eor.w    d0, (a7)+                   ; 00056a: b15f
    dbra     d5, L00056a                 ; 00056c: 51cdfffc
    move.l   -$6da(a6), d1               ; 000570: 222ef926
    eori.w   #$ead1, d1                  ; 000574: 0a41ead1
    sub.l    d1, -$6dc(a6)               ; 000578: 93aef924
    sub.w    d0, -$6da(a6)               ; 00057c: 916ef926
    movea.w  -$6da(a6), a7               ; 000580: 3e6ef926
    move.w   d5, sr                      ; 000584: 46c5
    move.w   sr, d0                      ; 000586: 40c0
    eor.w    d5, d0                      ; 000588: bb40
    or.w     d5, d0                      ; 00058a: 8045
    move.w   d0, -$6dc(a6)               ; 00058c: 3d40f924
    move.w   d0, d7                      ; 000590: 3e00
    move.w   #$2300, sr                  ; 000592: 46fc2300
    bra.w    $7ce                        ; 000596: 60000236
    dc.b $00,$00,$00,$00,$00,$00                                          ; 00059a ......
