@ Insertion sort
@ Sorts 10 random words.
@ category: algorithms
@ expect: halted, r1=1 r2=165 r3=137 r4=6 r5=16408 r6=104 r9=16420 r10=16384 r12=137 r13=40

    mov  r10, 0x4000
    mov  r12, 7
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
    mov  r1, 1              @ i
outer:
    lsl  r2, r1, 2
    add  r2, r2, r10
    ld   r3, 0[r2]          @ key
    sub  r4, r1, 1          @ j
inner:
    cmp  r4, -1
    beq  place
    lsl  r5, r4, 2
    add  r5, r5, r10
    ld   r6, 0[r5]
    cmp  r6, r3
    bgt  shift
    b    place
shift:
    st   r6, 4[r5]
    sub  r4, r4, 1
    b    inner
place:
    lsl  r5, r4, 2
    add  r5, r5, r10
    st   r3, 4[r5]
    add  r1, r1, 1
    cmp  r1, 10
    beq  done
    b    outer
done:
    ld   r1, 0[r10]
    ld   r2, 36[r10]
    hlt
