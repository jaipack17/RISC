@ Factorial, iterative
@ 10! = 3628800.
@ category: algorithms
@ expect: halted, r1=1 r2=3628800

    mov  r1, 10
    mov  r2, 1
loop:
    mul  r2, r2, r1
    sub  r1, r1, 1
    cmp  r1, 1
    bgt  loop
    hlt
