@ RAW hazard on rs1, distance 4
@ add r1 produces a value that the instruction 4 later reads as its first operand (read in OF /
@ OF1). 20 iterations, same instruction count for every distance.
@ category: hazards
@ expect: halted, r1=20 r2=20 r5=1 r6=2 r7=3

    mov  r11, 20
loop:
    add  r1, r1, 1
    mov  r5, 1
    mov  r6, 2
    mov  r7, 3
    add  r2, r1, 0
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
