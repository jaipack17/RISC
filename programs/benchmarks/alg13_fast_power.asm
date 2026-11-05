@ Fast exponentiation
@ 3^13 = 1594323 by square-and-multiply.
@ category: algorithms
@ expect: halted, r1=43046721 r3=1594323 r4=1

    mov  r1, 3              @ base
    mov  r2, 13             @ exponent
    mov  r3, 1              @ result
loop:
    cmp  r2, 0
    beq  done
    and  r4, r2, 1
    cmp  r4, 1
    beq  multiply
    b    square
multiply:
    mul  r3, r3, r1
square:
    mul  r1, r1, r1
    lsr  r2, r2, 1
    b    loop
done:
    hlt
