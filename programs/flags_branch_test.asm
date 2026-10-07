@ Exercise carry and overflow branch paths in the vertical microcode.
main:
    mov  r6, 1
    add  r7, r6, 1       @ C=0, V=0
    bc   failed          @ must not branch
    bv   failed          @ must not branch

    mov  r1, -1
    add  r2, r1, 1       @ unsigned carry, no signed overflow
    bc   carry_ok        @ must branch
    b    failed

carry_ok:
    movh r3, 0x7fff
    oru  r3, r3, 0xffff  @ r3 = 0x7fffffff
    add  r4, r3, 1       @ signed overflow
    bv   overflow_ok     @ must branch
    b    failed

overflow_ok:
    mov  r5, 1
    hlt

failed:
    mov  r5, 0
    hlt
