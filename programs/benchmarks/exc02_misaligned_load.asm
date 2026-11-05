@ MisalignedAccess
@ ld from an address that is not a multiple of 4.
@ category: exceptions
@ expect: exception MisalignedAccess at 0x0010, r1=42 r2=42 r10=16384

    mov  r10, 0x4000
    mov  r1, 42
    st   r1, 0[r10]
    ld   r2, 0[r10]
    ld   r3, 2[r10]
    mov  r4, 1
    hlt
