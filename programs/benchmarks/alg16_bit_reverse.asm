@ Reverse 16 bits
@ Reverses the 16 low bits of 0x1234 (0x2C48).
@ category: algorithms
@ expect: halted, r2=11336

    movu r1, 0x1234
    mov  r2, 0
    mov  r3, 16
loop:
    lsl  r2, r2, 1
    and  r4, r1, 1
    or   r2, r2, r4
    lsr  r1, r1, 1
    sub  r3, r3, 1
    cmp  r3, 0
    bgt  loop
    hlt
