@ Fletcher-16 checksum
@ Fletcher-16 of 16 random words (sums mod 255).
@ category: algorithms
@ expect: halted, r1=64 r2=81 r3=155 r4=16444 r5=29 r6=39761 r9=16444 r10=16384 r12=29 r13=64

    mov  r10, 0x4000
    mov  r12, 13
    mov  r13, 0
fill:
    mul  r12, r12, 13
    add  r12, r12, 7
    and  r12, r12, 255
    add  r9, r10, r13
    st   r12, 0[r9]
    add  r13, r13, 4
    cmp  r13, 64
    beq  filled
    b    fill
filled:
    mov  r1, 0
    mov  r2, 0              @ sum1
    mov  r3, 0              @ sum2
loop:
    add  r4, r10, r1
    ld   r5, 0[r4]
    add  r2, r2, r5
    mod  r2, r2, 255
    add  r3, r3, r2
    mod  r3, r3, 255
    add  r1, r1, 4
    cmp  r1, 64
    beq  done
    b    loop
done:
    lsl  r6, r3, 8
    or   r6, r6, r2
    hlt
