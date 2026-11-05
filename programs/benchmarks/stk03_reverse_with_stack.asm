@ Reverse an array with the stack
@ Pushes 12 random words, then pops them back in place.
@ category: stack
@ expect: halted, r1=48 r2=46 r3=16428 r5=15 r6=46 r9=16428 r10=16384 r12=15 r13=48

    mov  r10, 0x4000
    mov  r12, 3
    mov  r13, 0
fill:
    mul  r12, r12, 13
    add  r12, r12, 7
    and  r12, r12, 255
    add  r9, r10, r13
    st   r12, 0[r9]
    add  r13, r13, 4
    cmp  r13, 48
    beq  filled
    b    fill
filled:
    mov  r1, 0
pushes:
    add  r3, r10, r1
    ld   r2, 0[r3]
    push r2
    add  r1, r1, 4
    cmp  r1, 48
    beq  pops
    b    pushes
pops:
    mov  r1, 0
again:
    pop  r2
    add  r3, r10, r1
    st   r2, 0[r3]
    add  r1, r1, 4
    cmp  r1, 48
    beq  done
    b    again
done:
    ld   r5, 0[r10]
    ld   r6, 44[r10]
    hlt
