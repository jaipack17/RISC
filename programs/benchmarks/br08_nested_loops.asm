@ Nested loops
@ sum of i*j for i, j in 1..8.
@ category: branches
@ expect: halted, r1=1296 r2=9 r3=9 r4=64

    mov  r1, 0
    mov  r2, 1
outer:
    mov  r3, 1
inner:
    mul  r4, r2, r3
    add  r1, r1, r4
    add  r3, r3, 1
    cmp  r3, 9
    beq  inner_done
    b    inner
inner_done:
    add  r2, r2, 1
    cmp  r2, 9
    beq  done
    b    outer
done:
    hlt
