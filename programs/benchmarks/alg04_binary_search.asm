@ Binary search
@ a[i] = 3i + 1 for 32 words; finds 61 (index 20) and then 62 (missing, -1).
@ category: algorithms
@ expect: halted, r1=21 r2=20 r3=20 r4=62 r5=-1 r6=16464 r7=61 r8=20 r9=-1 r10=16384 r15=64

    mov  r10, 0x4000
    mov  r1, 0
fill:
    mul  r2, r1, 3
    add  r2, r2, 1
    lsl  r3, r1, 2
    add  r3, r3, r10
    st   r2, 0[r3]
    add  r1, r1, 1
    cmp  r1, 32
    beq  filled
    b    fill
filled:
    mov  r4, 61
    call search
    mov  r8, r5
    mov  r4, 62
    call search
    mov  r9, r5
    hlt
@ r5 = index of r4 in a[0..31], or -1
search:
    mov  r1, 0              @ low
    mov  r2, 31             @ high
probe:
    cmp  r1, r2
    bgt  missing
    add  r3, r1, r2
    div  r3, r3, 2
    lsl  r6, r3, 2
    add  r6, r6, r10
    ld   r7, 0[r6]
    cmp  r7, r4
    beq  found
    bgt  go_left
    add  r1, r3, 1
    b    probe
go_left:
    sub  r2, r3, 1
    b    probe
found:
    mov  r5, r3
    ret
missing:
    mov  r5, -1
    ret
