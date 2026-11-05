@ if / else-if chain
@ A switch on i mod 4 for i = 0..31 written as a chain of compares.
@ category: branches
@ expect: halted, r1=32 r2=3 r5=8 r6=120 r7=-8 r8=136

    mov  r1, 0
    mov  r5, 0
    mov  r6, 0
    mov  r7, 0
    mov  r8, 0
next:
    mod  r2, r1, 4
    cmp  r2, 0
    beq  case0
    cmp  r2, 1
    beq  case1
    cmp  r2, 2
    beq  case2
    add  r8, r8, r1
    b    join
case0:
    add  r5, r5, 1
    b    join
case1:
    add  r6, r6, r1
    b    join
case2:
    sub  r7, r7, 1
join:
    add  r1, r1, 1
    cmp  r1, 32
    beq  done
    b    next
done:
    hlt
