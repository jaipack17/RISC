@ Dot product
@ Dot product of two 16-word random vectors.
@ category: algorithms
@ expect: halted, r1=64 r2=252712 r3=16700 r4=5439 r5=37 r9=16700 r10=16384 r11=16640 r12=37 r13=64

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
    cmp  r13, 64
    beq  filled
    b    fill
filled:
    movu r11, 0x4100
    mov  r12, 21
    mov  r13, 0
fill_b:
    mul  r12, r12, 13
    add  r12, r12, 7
    and  r12, r12, 255
    add  r9, r11, r13
    st   r12, 0[r9]
    add  r13, r13, 4
    cmp  r13, 64
    beq  dot
    b    fill_b
dot:
    mov  r1, 0
    mov  r2, 0
loop:
    add  r3, r10, r1
    ld   r4, 0[r3]
    add  r3, r11, r1
    ld   r5, 0[r3]
    mul  r4, r4, r5
    add  r2, r2, r4
    add  r1, r1, 4
    cmp  r1, 64
    beq  done
    b    loop
done:
    hlt
