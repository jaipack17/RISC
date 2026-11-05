@ Search with an early exit
@ Fills 64 words with 5*i and stops at the first element equal to 125 (index 25).
@ category: branches
@ expect: halted, r1=25 r2=125 r3=16484 r10=16384

    mov  r10, 0x4000
    mov  r1, 0
fill:
    mul  r2, r1, 5
    lsl  r3, r1, 2
    add  r3, r3, r10
    st   r2, 0[r3]
    add  r1, r1, 1
    cmp  r1, 64
    beq  search
    b    fill
search:
    mov  r1, 0
look:
    lsl  r3, r1, 2
    add  r3, r3, r10
    ld   r2, 0[r3]
    cmp  r2, 125
    beq  found
    add  r1, r1, 1
    b    look
found:
    hlt
