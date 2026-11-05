@ WriteToText
@ st into the read-only instruction region.
@ category: exceptions
@ expect: exception WriteToText at 0x000C, r1=5 r10=16384

    mov  r1, 5
    mov  r10, 0x4000
    st   r1, 0[r10]
    st   r1, 8[r0]
    mov  r4, 1
    hlt
