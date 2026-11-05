@ Recursive Fibonacci
@ fib(10) = 55 with two recursive calls per level.
@ category: stack
@ expect: halted, r1=10 r2=55 r3=34 r15=8

    mov  r1, 10
    call fib
    hlt
fib:
    cmp  r1, 2
    bgt  recurse
    mov  r2, 1
    ret
recurse:
    push ra
    push r1
    sub  r1, r1, 1
    call fib
    pop  r1
    push r2
    push r1
    sub  r1, r1, 2
    call fib
    pop  r1
    pop  r3
    add  r2, r2, r3
    pop  ra
    ret
