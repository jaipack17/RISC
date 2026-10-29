@ Factorial: computes n! for n = 5, leaves it in r2 and stores it at 0x4000

main:
    mov  r1, 5              @ r1 = n

    mov  r2, 1              @ r2 = result

loop:
    cmp  r1, 1
    bgt  multiply           @ keep going while n > 1
    b    done

multiply:
    mul  r2, r2, r1         @ result = result * n
    sub  r1, r1, 1          @ n = n - 1
    b    loop

done:
    mov  r3, 0x4000
    st   r2, 0[r3]          @ memory[0x4000] = n!
    hlt
    
