@ Primes by trial division
@ Counts the primes up to 50 (15) using mod.
@ category: algorithms
@ expect: halted, r1=51 r2=2 r3=4 r5=15

    mov  r1, 2
    mov  r5, 0
candidate:
    mov  r2, 2
divisor:
    mul  r3, r2, r2
    cmp  r3, r1
    bgt  prime
    mod  r4, r1, r2
    cmp  r4, 0
    beq  not_prime
    add  r2, r2, 1
    b    divisor
prime:
    add  r5, r5, 1
not_prime:
    add  r1, r1, 1
    cmp  r1, 51
    beq  done
    b    candidate
done:
    hlt
