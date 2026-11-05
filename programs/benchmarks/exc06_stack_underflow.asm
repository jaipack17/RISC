@ StackUnderflow
@ One pop more than the pushes: sp would go past 0x10000.
@ category: exceptions
@ expect: exception StackUnderflow at 0x0028, r1=3 r2=4 r3=4 r4=3

    mov  r1, 3
    mov  r2, 4
    push r1
    push r2
    pop  r3
    pop  r4
    pop  r5
    mov  r6, 1
    hlt
