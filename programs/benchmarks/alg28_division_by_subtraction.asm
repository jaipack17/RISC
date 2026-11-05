@ Division by repeated subtraction
@ 1000 / 7 = 142 remainder 6 without div.
@ category: algorithms
@ expect: halted, r1=6 r2=7 r3=142

    mov  r1, 1000
    mov  r2, 7
    mov  r3, 0
loop:
    cmp  r2, r1
    bgt  done
    sub  r1, r1, r2
    add  r3, r3, 1
    b    loop
done:
    hlt
