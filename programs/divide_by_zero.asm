@ Precise exceptions: the div raises DivideByZero.
@ Everything before it finishes (r1, r2 and the store), nothing after it has any effect.

    mov  r10, 0x4000
    mov  r1, 42
    mov  r2, 0
    st   r1, 0[r10]         @ happens
    div  r3, r1, r2         @ DivideByZero at pc 0x0010
    st   r1, 4[r10]         @ never happens
    mov  r4, 99             @ never happens
    hlt
