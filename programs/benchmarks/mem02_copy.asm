@ Copy an array
@ Copies 32 random words from 0x4000 to 0x4200.
@ category: memory
@ expect: halted, r1=128 r2=35 r3=16508 r4=17020 r5=35 r9=16508 r10=16384 r11=16896 r12=35 r13=128

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
    cmp  r13, 128
    beq  filled
    b    fill
filled:
    movu r11, 0x4200
    mov  r1, 0
copy:
    add  r3, r10, r1
    ld   r2, 0[r3]
    add  r4, r11, r1
    st   r2, 0[r4]
    add  r1, r1, 4
    cmp  r1, 128
    beq  copied
    b    copy
copied:
    ld   r5, 124[r11]
    hlt
