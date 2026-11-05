@ Mutual recursion
@ is_even(n) and is_odd(n) call each other: is_even(25) = 0, is_odd(25) = 1.
@ category: stack
@ expect: halted, r2=1 r6=1 r15=20

    mov  r1, 25
    call is_even
    mov  r5, r2
    mov  r1, 25
    call is_odd
    mov  r6, r2
    hlt
is_even:
    cmp  r1, 0
    bgt  even_more
    mov  r2, 1
    ret
even_more:
    push ra
    sub  r1, r1, 1
    call is_odd
    pop  ra
    ret
is_odd:
    cmp  r1, 0
    bgt  odd_more
    mov  r2, 0
    ret
odd_more:
    push ra
    sub  r1, r1, 1
    call is_even
    pop  ra
    ret
