@ Collatz steps
@ Steps for 27 to reach 1 (111): a data-dependent branch every step.
@ category: algorithms
@ expect: halted, r1=1 r2=111

    mov  r1, 27
    mov  r2, 0
loop:
    cmp  r1, 1
    beq  done
    and  r3, r1, 1
    cmp  r3, 1
    beq  odd
    lsr  r1, r1, 1
    b    count
odd:
    mul  r1, r1, 3
    add  r1, r1, 1
count:
    add  r2, r2, 1
    b    loop
done:
    hlt
