@ xorshift random numbers
@ 10 rounds of x ^= x << 13; x ^= x >> 17; x ^= x << 5, from x = 2463534242.
@ category: algorithms
@ expect: halted, r1=-1641071047 r3=932422432 r5=36002

    movh r1, 0x92D6
    movu r5, 0x8CA2
    or   r1, r1, r5
    mov  r2, 10
loop:
    lsl  r3, r1, 13
    xor  r1, r1, r3
    lsr  r3, r1, 17
    xor  r1, r1, r3
    lsl  r3, r1, 5
    xor  r1, r1, r3
    sub  r2, r2, 1
    cmp  r2, 0
    bgt  loop
    hlt
