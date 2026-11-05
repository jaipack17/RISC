@ Histogram
@ Counts 32 random values by their low 3 bits into 8 buckets at 0x4400.
@ category: memory
@ expect: halted, r1=128 r3=16508 r4=17416 r5=4 r6=4 r9=16508 r10=16384 r11=17408 r12=170 r13=128

    mov  r10, 0x4000
    mov  r12, 10
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
    movu r11, 0x4400
    mov  r1, 0
count:
    add  r3, r10, r1
    ld   r4, 0[r3]
    and  r4, r4, 7
    lsl  r4, r4, 2
    add  r4, r4, r11
    ld   r5, 0[r4]
    add  r5, r5, 1
    st   r5, 0[r4]
    add  r1, r1, 4
    cmp  r1, 128
    beq  done
    b    count
done:
    ld   r6, 12[r11]
    hlt
