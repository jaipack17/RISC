@ String length
@ Length of "hello, world" stored one character per word, 0-terminated (12).
@ category: algorithms
@ expect: halted, r2=12 r3=16432 r10=16384

    mov  r10, 0x4000
    mov  r1, 104
    st   r1, 0[r10]
    mov  r1, 101
    st   r1, 4[r10]
    mov  r1, 108
    st   r1, 8[r10]
    mov  r1, 108
    st   r1, 12[r10]
    mov  r1, 111
    st   r1, 16[r10]
    mov  r1, 44
    st   r1, 20[r10]
    mov  r1, 32
    st   r1, 24[r10]
    mov  r1, 119
    st   r1, 28[r10]
    mov  r1, 111
    st   r1, 32[r10]
    mov  r1, 114
    st   r1, 36[r10]
    mov  r1, 108
    st   r1, 40[r10]
    mov  r1, 100
    st   r1, 44[r10]
    mov  r2, 0
    mov  r3, r10
loop:
    ld   r1, 0[r3]
    cmp  r1, 0
    beq  done
    add  r2, r2, 1
    add  r3, r3, 4
    b    loop
done:
    hlt
