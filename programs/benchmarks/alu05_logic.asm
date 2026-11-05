@ Logic operations
@ and, or, xor, not on registers and immediates.
@ category: alu
@ expect: halted, r1=61680 r2=4095 r3=2730 r4=45055 r5=42325 r6=-42326

    movu r1, 0xF0F0
    movu r2, 0x0FF0
    mov  r11, 16
loop:
    and  r3, r1, r2
    or   r4, r1, r2
    xor  r5, r3, r4
    not  r6, r5
    xor  r1, r1, 0x5A5A
    or   r2, r2, r11
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  loop
    hlt
