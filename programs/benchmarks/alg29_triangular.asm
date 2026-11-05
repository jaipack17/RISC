@ Triangular number
@ 1 + 2 + ... + 100 = 5050.
@ category: algorithms
@ expect: halted, r2=5050

    mov  r1, 100
    mov  r2, 0
loop:
    add  r2, r2, r1
    sub  r1, r1, 1
    cmp  r1, 0
    bgt  loop
    hlt
