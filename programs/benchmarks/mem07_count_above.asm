@ Count elements above a threshold
@ How many of 32 random words are > 128.
@ category: memory
@ expect: halted, r1=128 r2=12 r3=16508 r4=164 r9=16508 r10=16384 r12=164 r13=128

    mov  r10, 0x4000
    mov  r12, 4
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
    mov  r1, 0
    mov  r2, 0
count:
    add  r3, r10, r1
    ld   r4, 0[r3]
    cmp  r4, 128
    bgt  above
    b    next
above:
    add  r2, r2, 1
next:
    add  r1, r1, 4
    cmp  r1, 128
    beq  done
    b    count
done:
    hlt
