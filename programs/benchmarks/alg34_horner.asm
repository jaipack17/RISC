@ Polynomial by Horner's rule
@ p(x) = 3x^4 - 2x^3 + 5x - 7 at x = 3 (197), coefficients in memory.
@ category: algorithms
@ expect: halted, r1=-7 r2=3 r3=197 r4=20 r5=16400 r6=-7 r10=16384

    mov  r10, 0x4000
    mov  r1, 3
    st   r1, 0[r10]
    mov  r1, -2
    st   r1, 4[r10]
    mov  r1, 0
    st   r1, 8[r10]
    mov  r1, 5
    st   r1, 12[r10]
    mov  r1, -7
    st   r1, 16[r10]
    mov  r2, 3              @ x
    mov  r3, 0              @ result
    mov  r4, 0
loop:
    mul  r3, r3, r2
    add  r5, r10, r4
    ld   r6, 0[r5]
    add  r3, r3, r6
    add  r4, r4, 4
    cmp  r4, 20
    beq  done
    b    loop
done:
    hlt
