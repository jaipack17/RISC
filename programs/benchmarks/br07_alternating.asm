@ Alternating branch
@ if (i is odd) r1 += i else r2 += i, for i = 0..31: the branch is taken every other iteration.
@ category: branches
@ expect: halted, r1=256 r2=240 r3=32 r4=1

    mov  r1, 0
    mov  r2, 0
    mov  r3, 0
next:
    and  r4, r3, 1
    cmp  r4, 1
    beq  odd
    add  r2, r2, r3
    b    join
odd:
    add  r1, r1, r3
join:
    add  r3, r3, 1
    cmp  r3, 32
    beq  done
    b    next
done:
    hlt
