@ Load-use, second operand
@ ld r1 then immediately add r2, r2, r1: r1 is read one stage later (EX / OF2), so the wait is
@ shorter.
@ category: load-use
@ expect: halted, r1=3 r2=60 r5=1 r6=2 r10=16384

    mov  r10, 0x4000
    mov  r1, 3
    st   r1, 0[r10]
    mov  r2, 0
    mov  r11, 20
loop:
    ld   r1, 0[r10]
    add  r2, r2, r1
    mov  r5, 1
    mov  r6, 2
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
