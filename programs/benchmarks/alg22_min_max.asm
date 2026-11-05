@ Minimum and maximum
@ The smallest and largest of 20 random words in one pass.
@ category: algorithms
@ expect: halted, r1=80 r2=2 r3=230 r4=16460 r5=197 r9=16460 r10=16384 r12=197 r13=80

    mov  r10, 0x4000
    mov  r12, 17
    mov  r13, 0
fill:
    mul  r12, r12, 13
    add  r12, r12, 7
    and  r12, r12, 255
    add  r9, r10, r13
    st   r12, 0[r9]
    add  r13, r13, 4
    cmp  r13, 80
    beq  filled
    b    fill
filled:
    ld   r2, 0[r10]         @ min
    mov  r3, r2             @ max
    mov  r1, 4
scan:
    add  r4, r10, r1
    ld   r5, 0[r4]
    cmp  r5, r3
    bgt  new_max
    cmp  r2, r5
    bgt  new_min
    b    next
new_max:
    mov  r3, r5
    b    next
new_min:
    mov  r2, r5
next:
    add  r1, r1, 4
    cmp  r1, 80
    beq  done
    b    scan
done:
    hlt
