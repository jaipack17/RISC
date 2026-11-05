@ Loop unrolled 4 times
@ The work of br03 (64 body executions) with the body unrolled 4 times: a quarter of the branches.
@ category: branches
@ expect: halted, r1=192 r2=64 r3=3

    mov  r1, 0
    mov  r2, 0
    mov  r3, 3
    mov  r11, 16
loop:
    add  r1, r1, r3
    add  r2, r2, 1
    add  r1, r1, r3
    add  r2, r2, 1
    add  r1, r1, r3
    add  r2, r2, 1
    add  r1, r1, r3
    add  r2, r2, 1
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
