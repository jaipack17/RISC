@ Fill an array
@ a[i] = 3*i for 64 words.
@ category: memory
@ expect: halted, r1=256 r2=192 r3=16636 r4=189 r10=16384

    mov  r10, 0x4000
    mov  r1, 0
    mov  r2, 0
fill:
    add  r3, r10, r1
    st   r2, 0[r3]
    add  r2, r2, 3
    add  r1, r1, 4
    cmp  r1, 256
    beq  done
    b    fill
done:
    ld   r4, 252[r10]
    hlt
