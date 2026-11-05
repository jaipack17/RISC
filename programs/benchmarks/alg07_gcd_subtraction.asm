@ gcd with subtraction
@ Euclid's original algorithm: gcd(1071, 462) = 21 by repeated subtraction.
@ category: algorithms
@ expect: halted, r1=21 r2=21

    mov  r1, 1071
    mov  r2, 462
loop:
    cmp  r1, r2
    beq  done
    bgt  a_bigger
    sub  r2, r2, r1
    b    loop
a_bigger:
    sub  r1, r1, r2
    b    loop
done:
    hlt
