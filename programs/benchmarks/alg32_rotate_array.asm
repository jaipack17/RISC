@ Rotate an array
@ Rotates 12 random words left by 3 places, using a copy at 0x4100.
@ category: algorithms
@ expect: halted, r1=12 r2=16392 r3=188 r4=16684 r5=147 r6=188 r9=16428 r10=16384 r11=16640 r12=91 r13=48

    mov  r10, 0x4000
    mov  r12, 15
    mov  r13, 0
fill:
    mul  r12, r12, 13
    add  r12, r12, 7
    and  r12, r12, 255
    add  r9, r10, r13
    st   r12, 0[r9]
    add  r13, r13, 4
    cmp  r13, 48
    beq  filled
    b    fill
filled:
    movu r11, 0x4100
    mov  r1, 0
rotate:
    add  r2, r1, 3
    mod  r2, r2, 12
    lsl  r2, r2, 2
    add  r2, r2, r10
    ld   r3, 0[r2]
    lsl  r4, r1, 2
    add  r4, r4, r11
    st   r3, 0[r4]
    add  r1, r1, 1
    cmp  r1, 12
    beq  done
    b    rotate
done:
    ld   r5, 0[r11]
    ld   r6, 44[r11]
    hlt
