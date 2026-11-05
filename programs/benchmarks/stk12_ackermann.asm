@ Ackermann function
@ A(2, 3) = 9: deep, irregular recursion.
@ category: stack
@ expect: halted, r1=2 r2=8 r3=9 r15=12

    mov  r1, 2
    mov  r2, 3
    call ack
    hlt
@ r3 = A(r1, r2)
ack:
    cmp  r1, 0
    bgt  m_positive
    add  r3, r2, 1
    ret
m_positive:
    cmp  r2, 0
    bgt  both_positive
    push ra
    sub  r1, r1, 1
    mov  r2, 1
    call ack
    add  r1, r1, 1
    pop  ra
    ret
both_positive:
    push ra
    push r1
    sub  r2, r2, 1
    call ack                @ A(m, n-1)
    pop  r1
    push r1
    sub  r1, r1, 1
    mov  r2, r3
    call ack                @ A(m-1, A(m, n-1))
    pop  r1
    pop  ra
    ret
