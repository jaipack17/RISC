@ Bubble sort
@ Sorts 10 random words; r1 = smallest, r2 = largest.
@ category: algorithms
@ expect: halted, r1=13 r2=247 r3=51 r8=1 r9=16420 r10=16384 r12=247 r13=40

    mov  r10, 0x4000
    mov  r12, 5
    mov  r13, 0
fill:
    mul  r12, r12, 13
    add  r12, r12, 7
    and  r12, r12, 255
    add  r9, r10, r13
    st   r12, 0[r9]
    add  r13, r13, 4
    cmp  r13, 40
    beq  filled
    b    fill
filled:
    mov  r7, 9
pass:
    mov  r1, r10
    mov  r8, 0
compare:
    ld   r2, 0[r1]
    ld   r3, 4[r1]
    cmp  r2, r3
    bgt  swap
    b    advance
swap:
    st   r3, 0[r1]
    st   r2, 4[r1]
advance:
    add  r1, r1, 4
    add  r8, r8, 1
    cmp  r7, r8
    bgt  compare
    sub  r7, r7, 1
    cmp  r7, 0
    bgt  pass
    ld   r1, 0[r10]
    ld   r2, 36[r10]
    hlt
