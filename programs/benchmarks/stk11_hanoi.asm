@ Towers of Hanoi
@ Counts the moves to solve 6 discs (63), with 4 values saved per call.
@ category: stack
@ expect: halted, r1=6 r2=1 r3=3 r4=2 r5=63 r6=2 r15=24

    mov  r1, 6              @ discs
    mov  r2, 1              @ from
    mov  r3, 3              @ to
    mov  r4, 2              @ via
    mov  r5, 0              @ moves
    call hanoi
    hlt
hanoi:
    cmp  r1, 0
    bgt  recurse
    ret
recurse:
    push ra
    push r1
    push r3
    push r4
    sub  r1, r1, 1
    mov  r6, r3
    mov  r3, r4
    mov  r4, r6
    call hanoi              @ n-1 discs from -> via
    pop  r4
    pop  r3
    pop  r1
    add  r5, r5, 1          @ move disc n from -> to
    push r1
    push r2
    push r3
    sub  r1, r1, 1
    mov  r6, r2
    mov  r2, r4
    mov  r4, r6
    call hanoi              @ n-1 discs via -> to
    pop  r3
    pop  r2
    pop  r1
    pop  ra
    ret
