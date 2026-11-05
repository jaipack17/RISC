@ Maximum of an array
@ The largest of 32 random words.
@ category: memory
@ expect: halted, r1=128 r2=255 r3=16508 r4=168 r9=16508 r10=16384 r12=168 r13=128

    mov  r10, 0x4000
    mov  r12, 8
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
    ld   r2, 0[r10]
    mov  r1, 4
scan:
    add  r3, r10, r1
    ld   r4, 0[r3]
    cmp  r4, r2
    bgt  bigger
    b    next
bigger:
    mov  r2, r4
next:
    add  r1, r1, 4
    cmp  r1, 128
    beq  done
    b    scan
done:
    hlt
