@ Hazard showcase for the pipeline view: every kind of stall and a flush.
@ Step it beat by beat and watch the interlock hold, bubble and flush.

    mov  r1, 5
    add  r2, r1, 1          @ rs1 needed right away: OF1 waits for mov r1
    add  r3, r2, r2         @ rs1 and rs2 both from the instruction before
    mov  r10, 0x4000
    st   r3, 0[r10]         @ st reads r3 in OF2
    ld   r4, 0[r10]
    add  r5, r4, r4         @ load-use: waits until the ld has written r4
    not  r6, r5             @ not reads its operand in OF2
    cmp  r6, r5
    bgt  skip               @ not taken
    b    over               @ taken: the 3 instructions behind it are flushed
    mov  r7, 111            @ flushed, never runs
    mov  r8, 222            @ flushed, never runs
skip:
    mov  r9, 333            @ never runs
over:
    mov  r11, 7             @ no dependency: no stall
    mov  r12, 8
    add  r13, r11, r12
    hlt
