@ A stack frame with locals
@ A function keeps three locals in its frame (sub sp / st / ld / add sp).
@ category: stack
@ expect: halted, r1=9 r2=2 r3=95 r4=18 r6=11 r15=28

    mov  r1, 5
    mov  r2, 7
    call combine
    mov  r6, r3
    mov  r1, 9
    mov  r2, 2
    call combine
    hlt
@ r3 = (a + b) * (a - b) + a * b, locals at 0[sp], 4[sp], 8[sp]
combine:
    sub  sp, sp, 12
    add  r4, r1, r2
    st   r4, 0[sp]
    sub  r4, r1, r2
    st   r4, 4[sp]
    mul  r4, r1, r2
    st   r4, 8[sp]
    ld   r3, 0[sp]
    ld   r4, 4[sp]
    mul  r3, r3, r4
    ld   r4, 8[sp]
    add  r3, r3, r4
    add  sp, sp, 12
    ret
