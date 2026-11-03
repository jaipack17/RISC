@ Count the 1 bits of a number with shifts and masks.
@ 0xB6D5 = 1011 0110 1101 0101 has 10 ones. Result: r2 = 10.

    movu r1, 0xB6D5         @ the number (movu: the immediate is zero-extended)
    mov  r2, 0              @ r2 = count

loop:
    cmp  r1, 0
    beq  done
    and  r3, r1, 1          @ lowest bit
    add  r2, r2, r3
    lsr  r1, r1, 1
    b    loop

done:
    hlt
