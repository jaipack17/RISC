@ Merge two sorted arrays
@ Merges 2, 5, 8, ... and 3, 4, 9, ... (8 words each) into 16 sorted words.
@ category: algorithms
@ expect: halted, r1=8 r2=8 r3=16 r4=16572 r5=31 r6=23 r7=2 r8=31 r10=16384 r11=16448 r12=16512

    mov  r10, 0x4000        @ a
    add  r11, r10, 64       @ b
    add  r12, r10, 128      @ out
    mov  r1, 0
fill:
    mul  r2, r1, 3
    add  r2, r2, 2
    lsl  r3, r1, 2
    add  r4, r10, r3
    st   r2, 0[r4]
    mul  r2, r1, 4
    add  r2, r2, 3
    add  r4, r11, r3
    st   r2, 0[r4]
    add  r1, r1, 1
    cmp  r1, 8
    beq  merge
    b    fill
merge:
    mov  r1, 0              @ i
    mov  r2, 0              @ j
    mov  r3, 0              @ k
step:
    cmp  r1, 8
    beq  take_b
    cmp  r2, 8
    beq  take_a
    lsl  r4, r1, 2
    add  r4, r4, r10
    ld   r5, 0[r4]
    lsl  r4, r2, 2
    add  r4, r4, r11
    ld   r6, 0[r4]
    cmp  r5, r6
    bgt  take_b
take_a:
    lsl  r4, r1, 2
    add  r4, r4, r10
    ld   r5, 0[r4]
    add  r1, r1, 1
    b    put
take_b:
    lsl  r4, r2, 2
    add  r4, r4, r11
    ld   r5, 0[r4]
    add  r2, r2, 1
put:
    lsl  r4, r3, 2
    add  r4, r4, r12
    st   r5, 0[r4]
    add  r3, r3, 1
    cmp  r3, 16
    beq  done
    b    step
done:
    ld   r7, 0[r12]
    ld   r8, 60[r12]
    hlt
