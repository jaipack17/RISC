@ Divide and modulo
@ div and mod by immediates and registers.
@ category: alu
@ expect: halted, r1=1998 r2=7 r3=285 r4=4 r5=5 r6=2

    movu r1, 60000
    mov  r2, 7
    mov  r11, 16
loop:
    div  r3, r1, r2
    mod  r4, r1, r2
    div  r1, r1, 2
    add  r1, r1, 999
    mod  r5, r3, 10
    div  r6, r4, 2
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
