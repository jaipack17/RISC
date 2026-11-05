@ DivideByZero
@ A division by zero in the middle of a loop.
@ category: exceptions
@ expect: exception DivideByZero at 0x0008, r1=100 r3=100 r4=208

    mov  r1, 100
    mov  r2, 4
loop:
    div  r3, r1, r2
    add  r4, r4, r3
    sub  r2, r2, 1
    b    loop
