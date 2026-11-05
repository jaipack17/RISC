@ Shift-and-add multiplication
@ 123 * 45 = 5535 without mul.
@ category: algorithms
@ expect: halted, r1=7872 r3=5535 r4=1

    mov  r1, 123
    mov  r2, 45
    mov  r3, 0
loop:
    cmp  r2, 0
    beq  done
    and  r4, r2, 1
    cmp  r4, 1
    beq  add_it
    b    shift
add_it:
    add  r3, r3, r1
shift:
    lsl  r1, r1, 1
    lsr  r2, r2, 1
    b    loop
done:
    hlt
