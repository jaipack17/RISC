@ Serial dependency chain
@ Every instruction depends on the one before it (distance 1, rs1): the worst case for an
@ interlocked pipeline.
@ category: hazards
@ expect: halted, r1=100

    mov  r11, 20
loop:
    add  r1, r1, 1
    add  r1, r1, 1
    add  r1, r1, 1
    add  r1, r1, 1
    add  r1, r1, 1
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
