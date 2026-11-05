@ Shifts
@ lsl, lsr and asr by immediates and registers.
@ category: alu
@ expect: halted, r1=131071 r2=-1024 r3=32767 r4=-128 r5=262136 r6=15 r7=3

    mov  r1, 1
    mov  r2, -1024
    mov  r7, 3
    mov  r11, 16
loop:
    lsl  r1, r1, 1
    or   r1, r1, 1
    lsr  r3, r1, 2
    asr  r4, r2, r7
    lsl  r5, r3, r7
    lsr  r6, r2, 28
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
