@ Sum of decimal digits
@ Digit sum of 59876 (35) with div and mod by 10.
@ category: algorithms
@ expect: halted, r2=35 r3=5

    movu r1, 59876
    mov  r2, 0
loop:
    cmp  r1, 0
    beq  done
    mod  r3, r1, 10
    add  r2, r2, r3
    div  r1, r1, 10
    b    loop
done:
    hlt
