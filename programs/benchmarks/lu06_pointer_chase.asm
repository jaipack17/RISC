@ Pointer chasing (linked list)
@ Builds a 12-node linked list (value, next) and walks it: every load address comes from the
@ previous load.
@ category: load-use
@ expect: halted, r2=16480 r3=121 r4=16480 r6=506 r10=16384

    mov  r10, 0x4000
    mov  r1, 0              @ i
    mov  r2, r10            @ node address
build:
    mul  r3, r1, r1
    st   r3, 0[r2]          @ value = i*i
    add  r4, r2, 8
    st   r4, 4[r2]          @ next
    add  r1, r1, 1
    mov  r2, r4
    cmp  r1, 12
    beq  built
    b    build
built:
    mov  r5, 0
    st   r5, -4[r2]         @ the last node's next = 0
    mov  r1, r10
    mov  r6, 0              @ sum
walk:
    ld   r3, 0[r1]
    add  r6, r6, r3
    ld   r1, 4[r1]
    cmp  r1, 0
    beq  done
    b    walk
done:
    hlt
