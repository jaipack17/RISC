@ No data hazards
@ Five independent mov instructions per iteration: only the loop counter creates hazards. Baseline
@ for the hz group.
@ category: hazards
@ expect: halted, r1=1 r2=2 r5=3 r6=4 r7=5

    mov  r11, 20
loop:
    mov  r1, 1
    mov  r2, 2
    mov  r5, 3
    mov  r6, 4
    mov  r7, 5
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
