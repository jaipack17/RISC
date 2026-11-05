@ A deep stack
@ Pushes 0..31, then pops all 32 and adds them up (496).
@ category: stack
@ expect: halted, r2=496

    mov  r1, 0
fill:
    push r1
    add  r1, r1, 1
    cmp  r1, 32
    beq  empty
    b    fill
empty:
    mov  r2, 0
drain:
    pop  r3
    add  r2, r2, r3
    sub  r1, r1, 1
    cmp  r1, 0
    bgt  drain
    hlt
