@ Multiplies
@ mul with registers and immediates, results masked to stay small.
@ category: alu
@ expect: halted, r1=833 r2=3714 r3=3 r4=21 r5=441

    mov  r1, 1
    mov  r2, 2
    mov  r3, 3
    mov  r11, 16
loop:
    mul  r1, r1, 3
    and  r1, r1, 1023
    mul  r2, r2, r3
    and  r2, r2, 4095
    mul  r4, r3, 7
    mul  r5, r4, r4
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
