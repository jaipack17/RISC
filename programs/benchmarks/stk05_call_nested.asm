@ Nested calls
@ f calls g calls h, each saving ra on the stack: f(x) = 2 * g(x), g(x) = h(x) + 1, h(x) = x * x.
@ category: stack
@ expect: halted, r1=8 r2=100 r5=296 r15=12

    mov  r1, 0
    mov  r5, 0
next:
    call f
    add  r5, r5, r2
    add  r1, r1, 1
    cmp  r1, 8
    beq  done
    b    next
done:
    hlt
f:
    push ra
    call g
    add  r2, r2, r2
    pop  ra
    ret
g:
    push ra
    call h
    add  r2, r2, 1
    pop  ra
    ret
h:
    mul  r2, r1, r1
    ret
