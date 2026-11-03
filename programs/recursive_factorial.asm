@ Recursive factorial using call, ret and the stack (sp = r14, ra = r15).
@ fact(n) = n * fact(n - 1), fact(1) = 1.  fact(6) = 720.
@ Result: r2 = 720, also stored at 0x4000.

    mov  r1, 6              @ argument n
    call fact
    mov  r10, 0x4000
    st   r2, 0[r10]
    hlt

@ fact: argument in r1, result in r2
fact:
    cmp  r1, 1
    bgt  recurse
    mov  r2, 1              @ fact(1) = 1
    ret
recurse:
    sub  sp, sp, 8          @ push ra and n
    st   ra, 0[sp]
    st   r1, 4[sp]
    sub  r1, r1, 1
    call fact               @ r2 = fact(n - 1)
    ld   r1, 4[sp]          @ pop n and ra
    ld   ra, 0[sp]
    add  sp, sp, 8
    mul  r2, r2, r1         @ r2 = n * fact(n - 1)
    ret
