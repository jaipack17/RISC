@ Fill data memory with 1, 2, 3, 4, 5, call sum, write the result at 0x4014.

main:
    mov  r1, 0x4000
    mov  r2, 1

fill:
    st   r2, 0[r1]
    add  r1, r1, 4
    add  r2, r2, 1
    cmp  r2, 6
    beq  done_fill
    b    fill

done_fill:
    call sum
    st   r3, 0[r1]
    hlt

sum:
    mov  r3, 0
    mov  r4, 0x4000

sum_loop:
    ld   r5, 0[r4]
    add  r3, r3, r5
    add  r4, r4, 4
    cmp  r4, r1
    beq  sum_done
    b    sum_loop

sum_done:
    ret
