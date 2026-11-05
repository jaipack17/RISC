@ Branches that are never taken
@ Four beq per iteration that always fall through: no flushes (predict not taken).
@ category: branches
@ expect: halted, r1=16 r2=16 r3=16

    mov  r1, 0
    mov  r11, 16
loop:
    cmp  r1, 99
    beq  never
    add  r1, r1, 1
    cmp  r1, 99
    beq  never
    add  r2, r2, 1
    cmp  r1, 99
    beq  never
    add  r3, r3, 1
    cmp  r1, 99
    beq  never
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
never:
    mov  r9, 1
    hlt
