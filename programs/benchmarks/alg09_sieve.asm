@ Sieve of Eratosthenes
@ Counts the primes below 64 (18) with a flag array.
@ category: algorithms
@ expect: halted, r1=64 r2=81 r3=16636 r4=1 r5=18 r10=16384

    mov  r10, 0x4000
    mov  r1, 2
outer:
    mul  r2, r1, r1
    cmp  r2, 64
    bgt  count
    lsl  r3, r1, 2
    add  r3, r3, r10
    ld   r4, 0[r3]
    cmp  r4, 0
    bgt  next
mark:
    cmp  r2, 63
    bgt  next
    lsl  r3, r2, 2
    add  r3, r3, r10
    mov  r4, 1
    st   r4, 0[r3]
    add  r2, r2, r1
    b    mark
next:
    add  r1, r1, 1
    b    outer
count:
    mov  r1, 2
    mov  r5, 0
tally:
    lsl  r3, r1, 2
    add  r3, r3, r10
    ld   r4, 0[r3]
    cmp  r4, 0
    bgt  composite
    add  r5, r5, 1
composite:
    add  r1, r1, 1
    cmp  r1, 64
    beq  done
    b    tally
done:
    hlt
