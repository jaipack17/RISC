@ Swap neighbouring pairs
@ Swaps a[2i] and a[2i+1] for 16 pairs.
@ category: memory
@ expect: halted, r1=16512 r3=249 r4=172 r5=78 r6=163 r9=16508 r10=16384 r12=172 r13=128

    mov  r10, 0x4000
    mov  r12, 12
    mov  r13, 0
fill:
    mul  r12, r12, 13
    add  r12, r12, 7
    and  r12, r12, 255
    add  r9, r10, r13
    st   r12, 0[r9]
    add  r13, r13, 4
    cmp  r13, 128
    beq  filled
    b    fill
filled:
    mov  r1, r10
    mov  r11, 16
swap:
    ld   r3, 0[r1]
    ld   r4, 4[r1]
    st   r4, 0[r1]
    st   r3, 4[r1]
    add  r1, r1, 8
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  swap
    ld   r5, 0[r10]
    ld   r6, 4[r10]
    hlt
