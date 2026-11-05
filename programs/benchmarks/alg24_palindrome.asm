@ Palindrome check
@ Checks 3 1 4 1 5 1 4 1 3 with two pointers (r5 = 1: palindrome).
@ category: algorithms
@ expect: halted, r1=16400 r2=16400 r3=1 r4=1 r5=1 r10=16384

    mov  r10, 0x4000
    mov  r1, 3
    st   r1, 0[r10]
    mov  r1, 1
    st   r1, 4[r10]
    mov  r1, 4
    st   r1, 8[r10]
    mov  r1, 1
    st   r1, 12[r10]
    mov  r1, 5
    st   r1, 16[r10]
    mov  r1, 1
    st   r1, 20[r10]
    mov  r1, 4
    st   r1, 24[r10]
    mov  r1, 1
    st   r1, 28[r10]
    mov  r1, 3
    st   r1, 32[r10]
    mov  r1, r10
    add  r2, r10, 32
    mov  r5, 1
check:
    cmp  r2, r1
    bgt  compare
    b    done
compare:
    ld   r3, 0[r1]
    ld   r4, 0[r2]
    cmp  r3, r4
    beq  inward
    mov  r5, 0
    b    done
inward:
    add  r1, r1, 4
    sub  r2, r2, 4
    b    check
done:
    hlt
