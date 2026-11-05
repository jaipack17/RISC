@ Recursive factorial
@ fact(7) = 5040 with push / pop of n and ra.
@ category: stack
@ expect: halted, r1=7 r2=5040 r15=8

    mov  r1, 7
    call fact
    hlt
fact:
    cmp  r1, 1
    bgt  recurse
    mov  r2, 1
    ret
recurse:
    push ra
    push r1
    sub  r1, r1, 1
    call fact
    pop  r1
    pop  ra
    mul  r2, r2, r1
    ret
