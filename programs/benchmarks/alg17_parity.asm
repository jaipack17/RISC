@ Parity by xor folding
@ Parity of 0x5A3C9E17 folded with shifts and xor (0 = even).
@ category: algorithms
@ expect: halted, r1=1814621157 r2=605575516 r3=1 r5=40471

    movh r1, 0x5A3C
    movu r5, 0x9E17
    or   r1, r1, r5
    lsr  r2, r1, 16
    xor  r1, r1, r2
    lsr  r2, r1, 8
    xor  r1, r1, r2
    lsr  r2, r1, 4
    xor  r1, r1, r2
    lsr  r2, r1, 2
    xor  r1, r1, r2
    lsr  r2, r1, 1
    xor  r1, r1, r2
    and  r3, r1, 1
    hlt
