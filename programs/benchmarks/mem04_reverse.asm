@ Reverse an array in place
@ Reverses 20 random words with two pointers.
@ category: memory
@ expect: halted, r1=16424 r2=16420 r3=192 r4=199 r5=202 r6=85 r9=16460 r10=16384 r12=202 r13=80

    mov  r10, 0x4000
    mov  r12, 6
    mov  r13, 0
fill:
    mul  r12, r12, 13
    add  r12, r12, 7
    and  r12, r12, 255
    add  r9, r10, r13
    st   r12, 0[r9]
    add  r13, r13, 4
    cmp  r13, 80
    beq  filled
    b    fill
filled:
    mov  r1, r10            @ left
    add  r2, r10, 76        @ right
swap:
    ld   r3, 0[r1]
    ld   r4, 0[r2]
    st   r4, 0[r1]
    st   r3, 0[r2]
    add  r1, r1, 4
    sub  r2, r2, 4
    cmp  r2, r1
    bgt  swap
    ld   r5, 0[r10]
    ld   r6, 76[r10]
    hlt
