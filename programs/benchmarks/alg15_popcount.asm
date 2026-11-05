@ Population count
@ Counts the 1 bits of 0xB6D5 (10) and 0x7FFF (15).
@ category: algorithms
@ expect: halted, r2=15 r3=1 r5=10 r6=15 r15=20

    movu r1, 0xB6D5
    call popcount
    mov  r5, r2
    movu r1, 0x7FFF
    call popcount
    mov  r6, r2
    hlt
popcount:
    mov  r2, 0
loop:
    cmp  r1, 0
    beq  done
    and  r3, r1, 1
    add  r2, r2, r3
    lsr  r1, r1, 1
    b    loop
done:
    ret
