@ gcd with modulo
@ Euclid's algorithm: gcd(3528, 3780) = 252.
@ category: algorithms
@ expect: halted, r1=252

    mov  r1, 3528
    mov  r2, 3780
loop:
    cmp  r2, 0
    beq  done
    mod  r3, r1, r2
    mov  r1, r2
    mov  r2, r3
    b    loop
done:
    hlt
