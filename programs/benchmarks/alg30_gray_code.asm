@ Gray codes
@ xor of gray(i) = i ^ (i >> 1) for i = 0..31, and their sum.
@ category: algorithms
@ expect: halted, r1=32 r3=496 r4=16

    mov  r1, 0
    mov  r2, 0
    mov  r3, 0
loop:
    lsr  r4, r1, 1
    xor  r4, r4, r1
    xor  r2, r2, r4
    add  r3, r3, r4
    add  r1, r1, 1
    cmp  r1, 32
    beq  done
    b    loop
done:
    hlt
