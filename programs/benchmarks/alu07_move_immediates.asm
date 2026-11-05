@ Immediate moves
@ mov / movu / movh: no register reads at all.
@ category: alu
@ expect: halted, r1=100 r2=50000 r3=305397760 r4=-7 r6=32767

    mov  r11, 16
loop:
    mov  r1, 100
    movu r2, 50000
    movh r3, 0x1234
    mov  r4, -7
    mov  r5, 0
    mov  r6, 32767
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
