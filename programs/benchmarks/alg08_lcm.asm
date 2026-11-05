@ Least common multiple
@ lcm(84, 126) = 252 = 84 / gcd * 126.
@ category: algorithms
@ expect: halted, r1=42 r4=252 r5=84 r6=126

    mov  r1, 84
    mov  r2, 126
    mov  r5, r1
    mov  r6, r2
loop:
    cmp  r2, 0
    beq  gcd_done
    mod  r3, r1, r2
    mov  r1, r2
    mov  r2, r3
    b    loop
gcd_done:
    div  r4, r5, r1
    mul  r4, r4, r6
    hlt
