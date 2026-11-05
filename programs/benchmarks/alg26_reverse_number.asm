@ Reverse a number
@ 12345 -> 54321.
@ category: algorithms
@ expect: halted, r2=54321 r3=1

    mov  r1, 12345
    mov  r2, 0
loop:
    cmp  r1, 0
    beq  done
    mod  r3, r1, 10
    mul  r2, r2, 10
    add  r2, r2, r3
    div  r1, r1, 10
    b    loop
done:
    hlt
