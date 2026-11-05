@ StackOverflow
@ Recursion that never stops: push past 0xC000.
@ category: exceptions
@ expect: exception StackOverflow at 0x000C, r1=2048 r14=49152 r15=36

    mov  r1, 0
    call forever
    hlt
forever:
    push ra
    push r1
    add  r1, r1, 1
    call forever
