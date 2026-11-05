# Benchmark programs

100 RISC201 programs for comparing the 4-stage and 6-stage pipelined microprogrammed
processors (assignment items 1a and 5). Every program runs on the 4-stage, 6-stage and
sequential microprogrammed processors and must end in the same state on all three:
`./build/risc_4stage_run programs/benchmarks/<name>.asm` checks this. The web app's
Compare page runs all of them (`GET /api/benchmark`).

Each file starts with a header that the web app reads:

```
@ title
@ what the program tests (one or more lines)
@ category: <category>
@ expect: halted, r1=... r2=...                 (registers that are not 0 at the end)
@ expect: exception <Kind> at 0x<pc>, r1=...   (for the exceptions category)
```

The expected results were computed with an independent Python model of the ISA, and all
100 match the real processors (registers, status, exception kind and pc).

| Prefix | Category | Programs | What it isolates |
|---|---|---|---|
| `hz` | hazards | 10 | RAW distance 1–4 on rs1 (read in OF / OF1) and rs2 (read in EX / OF2), no hazards, a serial chain |
| `lu` | load-use | 6 | a load followed by its use at distance 1–3, load→store, pointer chasing |
| `alu` | alu | 8 | register vs immediate adds, mul, div/mod, logic, shifts, immediate moves, a mix |
| `br` | branches | 12 | the same work as loops of 1, 4, 16 or 64 iterations; never taken, always taken, alternating, nested, if/else chains, early exit, unrolled |
| `mem` | memory | 10 | fill, copy, sum, reverse, strided access, max, counting, swapping, prefix sums, histogram |
| `stk` | stack | 14 | push / pop, a deep stack, leaf and nested calls, a frame with locals, recursion (factorial, Fibonacci, sum, power, Hanoi, Ackermann, gcd, mutual recursion) |
| `alg` | algorithms | 34 | sorting, searching, number theory, bit tricks, checksums, matrix multiply, strings, polynomials |
| `exc` | exceptions | 6 | DivideByZero, MisalignedAccess, OutOfBoundsAccess, WriteToText, StackOverflow, StackUnderflow |

The stack programs use the `push rX` / `pop rX` macros (full-descending stack on sp = r14):
`push` is `sub sp, sp, 4` + `st rX, 0[sp]`, `pop` is `add sp, sp, 4` + `ld rX, -4[sp]`.
sp must stay between 0xC000 (full) and 0x10000 (empty); a write that would move it outside
raises StackOverflow or StackUnderflow, precisely, on every processor.
