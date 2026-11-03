@ Fibonacci: writes the first 12 Fibonacci numbers to 0x4000, 0x4004, ...
@ Result: memory 0x4000..0x402C = 0 1 1 2 3 5 8 13 21 34 55 89, r3 = 89

    mov  r10, 0x4000        @ r10 = where the next number goes
    mov  r1, 0              @ r1 = F(n-2)
    mov  r2, 1              @ r2 = F(n-1)
    mov  r4, 10             @ 10 more numbers after the first two

    st   r1, 0[r10]
    st   r2, 4[r10]
    add  r10, r10, 8

next:
    add  r3, r1, r2         @ F(n) = F(n-2) + F(n-1): needs both, so it waits
    st   r3, 0[r10]
    mov  r1, r2
    mov  r2, r3
    add  r10, r10, 4
    sub  r4, r4, 1
    cmp  r4, 0
    bgt  next

    hlt
