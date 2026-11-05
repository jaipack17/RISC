@ OutOfBoundsAccess
@ ld past the end of the 64 KB memory.
@ category: exceptions
@ expect: exception OutOfBoundsAccess at 0x000C, r1=65520 r2=7

    movu r1, 0xFFF0
    mov  r2, 7
    ld   r3, 8[r1]
    ld   r3, 16[r1]
    mov  r4, 1
    hlt
