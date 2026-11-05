@ push and pop
@ Pushes r1..r4 and pops them into r5..r8 (reversed order).
@ category: stack
@ expect: halted, r1=11 r2=22 r3=33 r4=44 r5=44 r6=33 r7=22 r8=11

    mov  r1, 11
    mov  r2, 22
    mov  r3, 33
    mov  r4, 44
    push r1
    push r2
    push r3
    push r4
    pop  r5
    pop  r6
    pop  r7
    pop  r8
    hlt
