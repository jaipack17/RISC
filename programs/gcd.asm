@ Greatest common divisor with Euclid's algorithm (uses mod).
@ gcd(1071, 462) = 21. Result: r1 = 21, also stored at 0x4000.

    mov  r1, 1071           @ a
    mov  r2, 462            @ b

loop:
    cmp  r2, 0
    beq  done               @ while b != 0
    mod  r3, r1, r2         @ r = a mod b
    mov  r1, r2             @ a = b
    mov  r2, r3             @ b = r
    b    loop

done:
    mov  r10, 0x4000
    st   r1, 0[r10]
    hlt
