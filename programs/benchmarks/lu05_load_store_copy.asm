@ Load then store the loaded value
@ Copies 16 words: st reads the loaded register as its data (rd, read in EX / OF2).
@ category: load-use
@ expect: halted, r1=64 r2=25 r3=16444 r4=16700 r5=124 r6=25 r9=16444 r10=16384 r11=16640 r12=25 r13=64

    mov  r10, 0x4000
    mov  r12, 9
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
    movu r11, 0x4100
    mov  r1, 0
copy:
    add  r3, r10, r1
    ld   r2, 0[r3]
    add  r4, r11, r1
    st   r2, 0[r4]
    add  r1, r1, 4
    cmp  r1, 64
    beq  copied
    b    copy
copied:
    ld   r5, 0[r11]
    ld   r6, 60[r11]
    hlt
