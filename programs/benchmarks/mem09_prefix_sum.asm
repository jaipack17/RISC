@ Prefix sums
@ a[i] = a[0] + ... + a[i] in place for 24 random words.
@ category: memory
@ expect: halted, r1=96 r2=3228 r3=16476 r4=223 r5=3228 r9=16476 r10=16384 r12=223 r13=96

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
    cmp  r13, 96
    beq  filled
    b    fill
filled:
    mov  r1, 4
    ld   r2, 0[r10]
prefix:
    add  r3, r10, r1
    ld   r4, 0[r3]
    add  r2, r2, r4
    st   r2, 0[r3]
    add  r1, r1, 4
    cmp  r1, 96
    beq  done
    b    prefix
done:
    ld   r5, 92[r10]
    hlt
