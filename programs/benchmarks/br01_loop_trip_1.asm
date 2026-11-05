@ Loop with 1 iteration
@ The same 64 executions of the loop body, as 64 outer × 1 inner iterations: shorter loops pay
@ more taken-branch flushes per unit of work.
@ category: branches
@ expect: halted, r1=192 r2=64 r3=3

    mov  r1, 0
    mov  r2, 0
    mov  r3, 3
    mov  r11, 64
outer:
    mov  r12, 1
inner:
    add  r1, r1, r3
    add  r2, r2, 1
    sub  r12, r12, 1
    cmp  r12, 0
    bgt  inner
    sub  r11, r11, 1
    cmp  r11, 0
    bgt  outer
    hlt
