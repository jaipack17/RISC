@ Selection sort
@ Sorts 10 random words.
@ category: algorithms
@ expect: halted, r1=23 r2=145 r3=10 r4=16416 r5=145 r6=16420 r7=124 r9=16420 r10=16384 r12=27 r13=40

    mov  r10, 0x4000
    mov  r12, 9
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
    mov  r1, 0              @ i
outer:
    mov  r2, r1             @ index of the minimum
    add  r3, r1, 1          @ j
scan:
    cmp  r3, 10
    beq  swap
    lsl  r4, r3, 2
    add  r4, r4, r10
    ld   r5, 0[r4]
    lsl  r6, r2, 2
    add  r6, r6, r10
    ld   r7, 0[r6]
    cmp  r7, r5
    bgt  smaller
    b    next
smaller:
    mov  r2, r3
next:
    add  r3, r3, 1
    b    scan
swap:
    lsl  r4, r1, 2
    add  r4, r4, r10
    lsl  r6, r2, 2
    add  r6, r6, r10
    ld   r5, 0[r4]
    ld   r7, 0[r6]
    st   r7, 0[r4]
    st   r5, 0[r6]
    add  r1, r1, 1
    cmp  r1, 9
    beq  done
    b    outer
done:
    ld   r1, 0[r10]
    ld   r2, 36[r10]
    hlt
