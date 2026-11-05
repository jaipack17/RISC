@ RAW hazard on rs2, distance 3
@ add r1 produces a value that the instruction 3 later reads as its second operand (read in EX /
@ OF2). 20 iterations.
@ category: hazards
@ expect: halted, r1=20 r2=20 r5=1 r6=2 r7=3

    mov  r11, 20
loop:
    add  r1, r1, 1
    mov  r5, 1
    mov  r6, 2
    add  r2, r0, r1
    mov  r7, 3
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
