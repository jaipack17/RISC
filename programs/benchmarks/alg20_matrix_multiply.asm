@ 3x3 matrix multiply
@ C = A * B with A[i][j] = i + j, B[i][j] = i * j + 1; r1 = trace(C).
@ category: algorithms
@ expect: halted, r1=48 r2=14 r3=31 r4=16544 r5=16480 r6=31 r7=20 r8=5 r10=16384 r11=16448 r12=16512

    mov  r10, 0x4000        @ A
    add  r11, r10, 64       @ B
    add  r12, r10, 128      @ C
    mov  r1, 0
init_i:
    mov  r2, 0
init_j:
    mul  r3, r1, 3
    add  r3, r3, r2
    lsl  r3, r3, 2
    add  r4, r1, r2
    add  r5, r10, r3
    st   r4, 0[r5]
    mul  r4, r1, r2
    add  r4, r4, 1
    add  r5, r11, r3
    st   r4, 0[r5]
    add  r2, r2, 1
    cmp  r2, 3
    beq  init_next
    b    init_j
init_next:
    add  r1, r1, 1
    cmp  r1, 3
    beq  multiply
    b    init_i
multiply:
    mov  r1, 0
row:
    mov  r2, 0
col:
    mov  r6, 0              @ sum
    mov  r3, 0              @ k
dot:
    mul  r4, r1, 3
    add  r4, r4, r3
    lsl  r4, r4, 2
    add  r4, r4, r10
    ld   r7, 0[r4]          @ A[i][k]
    mul  r4, r3, 3
    add  r4, r4, r2
    lsl  r4, r4, 2
    add  r4, r4, r11
    ld   r8, 0[r4]          @ B[k][j]
    mul  r7, r7, r8
    add  r6, r6, r7
    add  r3, r3, 1
    cmp  r3, 3
    beq  store
    b    dot
store:
    mul  r4, r1, 3
    add  r4, r4, r2
    lsl  r4, r4, 2
    add  r4, r4, r12
    st   r6, 0[r4]
    add  r2, r2, 1
    cmp  r2, 3
    beq  next_row
    b    col
next_row:
    add  r1, r1, 1
    cmp  r1, 3
    beq  trace
    b    row
trace:
    ld   r1, 0[r12]
    ld   r2, 16[r12]
    ld   r3, 32[r12]
    add  r1, r1, r2
    add  r1, r1, r3
    hlt
