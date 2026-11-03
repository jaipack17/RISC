@ Bubble sort: sorts 8 numbers in memory at 0x4000 into increasing order.
@ Before: 29 3 17 8 42 1 23 11   After: 1 3 8 11 17 23 29 42

    mov  r10, 0x4000

    @ the unsorted numbers
    mov  r1, 29
    st   r1, 0[r10]
    mov  r1, 3
    st   r1, 4[r10]
    mov  r1, 17
    st   r1, 8[r10]
    mov  r1, 8
    st   r1, 12[r10]
    mov  r1, 42
    st   r1, 16[r10]
    mov  r1, 1
    st   r1, 20[r10]
    mov  r1, 23
    st   r1, 24[r10]
    mov  r1, 11
    st   r1, 28[r10]

    mov  r7, 7              @ r7 = pairs to compare in this pass

pass:
    mov  r1, r10            @ r1 = address of the left element of the pair
    mov  r8, 0              @ r8 = pairs compared so far
compare:
    ld   r2, 0[r1]
    ld   r3, 4[r1]
    cmp  r2, r3
    bgt  swap               @ left > right: swap them
    b    advance
swap:
    st   r3, 0[r1]
    st   r2, 4[r1]
advance:
    add  r1, r1, 4
    add  r8, r8, 1
    cmp  r7, r8
    bgt  compare            @ more pairs in this pass
    sub  r7, r7, 1          @ the largest is now at the end
    cmp  r7, 0
    bgt  pass

    hlt
