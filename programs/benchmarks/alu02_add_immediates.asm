@ Register-immediate adds
@ Six independent add r, r, imm per iteration: immediates need no second register read.
@ category: alu
@ expect: halted, r1=16 r2=32 r3=48 r4=64 r5=80 r6=96

    mov  r11, 16
loop:
    add  r1, r1, 1
    add  r2, r2, 2
    add  r3, r3, 3
    add  r4, r4, 4
    add  r5, r5, 5
    add  r6, r6, 6
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
