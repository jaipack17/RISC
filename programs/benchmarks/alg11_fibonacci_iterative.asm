@ Fibonacci, iterative
@ fib(20) = 6765.
@ category: algorithms
@ expect: halted, r1=4181 r2=6765 r4=6765

    mov  r1, 0
    mov  r2, 1
    mov  r3, 19
loop:
    add  r4, r1, r2
    mov  r1, r2
    mov  r2, r4
    sub  r3, r3, 1
    cmp  r3, 0
    bgt  loop
    hlt
