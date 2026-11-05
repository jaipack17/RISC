@ Recursive sum
@ sum(20) = 1 + ... + 20 = 210, recursion depth 20.
@ category: stack
@ expect: halted, r1=20 r2=210 r15=8

    mov  r1, 20
    call sum
    hlt
sum:
    cmp  r1, 0
    bgt  recurse
    mov  r2, 0
    ret
recurse:
    push ra
    push r1
    sub  r1, r1, 1
    call sum
    pop  r1
    pop  ra
    add  r2, r2, r1
    ret
