@ Calls to a leaf function
@ sum of square(i) for i = 1..10 (385); square does not touch the stack.
@ category: stack
@ expect: halted, r1=11 r2=100 r3=385 r15=12

    mov  r1, 1
    mov  r3, 0
next:
    call square
    add  r3, r3, r2
    add  r1, r1, 1
    cmp  r1, 11
    beq  done
    b    next
done:
    hlt
square:
    mul  r2, r1, r1
    ret
