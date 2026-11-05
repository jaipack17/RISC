@ Integer square root
@ isqrt(1000) = 31: the largest i with i*i <= n.
@ category: algorithms
@ expect: halted, r1=1000 r2=31 r3=32 r4=1024

    movu r1, 1000
    mov  r2, 0
loop:
    add  r3, r2, 1
    mul  r4, r3, r3
    cmp  r4, r1
    bgt  done
    mov  r2, r3
    b    loop
done:
    hlt
