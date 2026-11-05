@ Mixed ALU work
@ A mix of arithmetic, logic and shifts with short dependencies.
@ category: alu
@ expect: halted, r1=246 r2=5 r3=259 r4=777 r5=1015 r6=507

    mov  r1, 17
    mov  r2, 5
    mov  r11, 16
loop:
    add  r3, r1, r2
    mul  r4, r3, 3
    xor  r5, r4, r1
    lsr  r6, r5, 1
    sub  r1, r6, r2
    and  r1, r1, 255
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
