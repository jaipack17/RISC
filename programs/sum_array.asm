@ Sum of an array: fills array[0..9] at 0x4000 with 1..10, then adds them up.
@ Result: r4 = 55, also stored at 0x4028 (just after the array).

    mov  r1, 0x4000         @ r1 = address of array[0]
    mov  r2, 1              @ r2 = next value to store
    mov  r3, 10             @ r3 = number of elements

fill:
    st   r2, 0[r1]          @ array[i] = i + 1
    add  r1, r1, 4
    add  r2, r2, 1
    cmp  r2, r3
    bgt  filled             @ stop after storing 10
    b    fill
filled:

    mov  r1, 0x4000
    mov  r4, 0              @ r4 = sum
    mov  r5, 0              @ r5 = index

sum:
    ld   r6, 0[r1]          @ load-use hazard: the add below waits for r6
    add  r4, r4, r6
    add  r1, r1, 4
    add  r5, r5, 1
    cmp  r3, r5
    bgt  sum                @ loop while index < 10

    st   r4, 0[r1]          @ memory[0x4028] = 55
    hlt
