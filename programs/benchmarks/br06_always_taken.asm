@ Unconditional jumps
@ A chain of b instructions that jump over one instruction each: every jump flushes the pipeline.
@ category: branches
@ expect: halted, r1=48

    mov  r1, 0
    mov  r11, 8
loop:
    b    j0
    mov  r9, 1
j0:
    add  r1, r1, 1
    b    j1
    mov  r9, 1
j1:
    add  r1, r1, 1
    b    j2
    mov  r9, 1
j2:
    add  r1, r1, 1
    b    j3
    mov  r9, 1
j3:
    add  r1, r1, 1
    b    j4
    mov  r9, 1
j4:
    add  r1, r1, 1
    b    j5
    mov  r9, 1
j5:
    add  r1, r1, 1
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
