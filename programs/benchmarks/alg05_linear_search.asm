@ Linear search
@ a[i] = 7i mod 50 for 40 words; first index of 35 (5).
@ category: algorithms
@ expect: halted, r1=5 r2=35 r3=16404 r5=5 r10=16384

    mov  r10, 0x4000
    mov  r1, 0
fill:
    mul  r2, r1, 7
    mod  r2, r2, 50
    lsl  r3, r1, 2
    add  r3, r3, r10
    st   r2, 0[r3]
    add  r1, r1, 1
    cmp  r1, 40
    beq  filled
    b    fill
filled:
    mov  r1, 0
    mov  r5, -1
look:
    lsl  r3, r1, 2
    add  r3, r3, r10
    ld   r2, 0[r3]
    cmp  r2, 35
    beq  found
    add  r1, r1, 1
    cmp  r1, 40
    beq  done
    b    look
found:
    mov  r5, r1
done:
    hlt
