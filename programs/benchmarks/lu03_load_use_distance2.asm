@ Load-use at distance 2
@ One independent instruction between the ld and its use.
@ category: load-use
@ expect: halted, r1=3 r2=60 r5=1 r6=2 r10=16384

    mov  r10, 0x4000
    mov  r1, 3
    st   r1, 0[r10]
    mov  r2, 0
    mov  r11, 20
loop:
    ld   r1, 0[r10]
    mov  r5, 1
    add  r2, r1, r2
    mov  r6, 2
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
