@ Register-register adds
@ Six independent add r, r, r per iteration (each register depends on itself 6 instructions back).
@ category: alu
@ expect: halted, r1=48 r2=48 r3=48 r4=48 r5=48 r6=48 r7=3

    mov  r7, 3
    mov  r11, 16
loop:
    add  r1, r1, r7
    add  r2, r2, r7
    add  r3, r3, r7
    add  r4, r4, r7
    add  r5, r5, r7
    add  r6, r6, r7
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
