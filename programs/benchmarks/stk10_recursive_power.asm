@ Recursive power
@ power(3, 9) = 19683 by repeated squaring, recursively.
@ category: stack
@ expect: halted, r1=3 r2=19683 r3=9 r4=1 r15=12

    mov  r1, 3
    mov  r3, 9
    call power
    hlt
@ r2 = r1 ^ r3
power:
    cmp  r3, 0
    bgt  recurse
    mov  r2, 1
    ret
recurse:
    push ra
    push r3
    div  r3, r3, 2
    call power
    pop  r3
    pop  ra
    mul  r2, r2, r2
    and  r4, r3, 1
    cmp  r4, 1
    beq  odd
    ret
odd:
    mul  r2, r2, r1
    ret
