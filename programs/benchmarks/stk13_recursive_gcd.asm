@ Recursive gcd
@ gcd(1071, 462) = 21 by recursion on (b, a mod b).
@ category: stack
@ expect: halted, r1=21 r3=21 r15=12

    mov  r1, 1071
    mov  r2, 462
    call gcd
    hlt
gcd:
    cmp  r2, 0
    bgt  recurse
    mov  r3, r1
    ret
recurse:
    push ra
    mod  r4, r1, r2
    mov  r1, r2
    mov  r2, r4
    call gcd
    pop  ra
    ret
