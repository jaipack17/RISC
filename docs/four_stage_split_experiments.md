# Four-stage pipeline: choosing which stages to merge (experiment, 2026-11-03)

The four-stage pipelined microprogrammed processor runs `microcode/vertical.masm`
unchanged, like the six-stage one. To get four stages, two pairs of neighbouring
stages of the six-stage pipeline (IF | OF1 | OF2 | EX | MA | RW) are merged. This
experiment measures every way of doing that.

## Method

A beat-level timing model, written in Python:

1. Each program is parsed and executed instruction by instruction. Wrong-path
   instructions (fetched after a taken branch) are fetched and occupy stages too.
2. For a grouping of the six stages into four, every instruction's section length
   in each group is the number of its microcode lines that run there. A line the
   six-stage splitter cuts into two parts (e.g. `mmov B, regVal, <add>` = OF2 part
   + EX part) is one microinstruction again when its two stages are merged.
3. The pipeline is stepped beat by beat with the six-stage CPU's rules:
   - Beat length = the longest section among the stages that run in the beat
     (at least 1 microcycle).
   - No forwarding: a reader waits until the writer has left the write-back
     stage. The stages up to the reader hold, and a bubble goes in after it.
   - A taken branch flushes every instruction in a stage before EX's stage.
   - Fetching stops while a `hlt` is in the pipeline.

**Validation:** with the six-stage grouping, the model gives exactly the
microcycles and beats of the real `risc_6stage_run` on all eight programs.

## Results: microcycles for all 10 groupings

| Stages | factorial | sum_array | fibonacci | gcd | bubble_sort | recursive_factorial | popcount | hazards | **Total** | beats | stall beats | flushed |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| sequential micro CPU | 245 | 1238 | 889 | 218 | 3059 | 675 | 933 | 142 | **7399** | | | |
| 6-stage IF \| OF1 \| OF2 \| EX \| MA \| RW | 146 | 671 | 403 | 119 | 1670 | 365 | 428 | 92 | **3894** | 1750 | 578 | 287 |
| **IF \| OF1 \| OF2+EX \| MA+RW** | **117** | **545** | **339** | **95** | **1373** | **297** | **373** | **74** | **3213** | 1263 | 203 | 231 |
| IF \| OF1 \| OF2+EX+MA \| RW | 118 | 546 | 361 | 96 | 1391 | 308 | 373 | 74 | 3267 | 1263 | 203 | 231 |
| IF \| OF1 \| OF2 \| EX+MA+RW | 149 | 624 | 377 | 105 | 1597 | 363 | 440 | 83 | 3738 | 1394 | 238 | 287 |
| IF \| OF1+OF2+EX \| MA \| RW | 134 | 716 | 490 | 115 | 1756 | 343 | 502 | 85 | 4141 | 1248 | 319 | 131 |
| IF \| OF1+OF2 \| EX+MA \| RW | 157 | 791 | 499 | 130 | 1926 | 406 | 538 | 89 | 4536 | 1379 | 319 | 231 |
| IF+OF1 \| OF2+EX \| MA \| RW | 160 | 776 | 492 | 126 | 1908 | 418 | 572 | 97 | 4549 | 1328 | 399 | 131 |
| IF \| OF1+OF2 \| EX \| MA+RW | 156 | 790 | 499 | 129 | 1989 | 405 | 538 | 90 | 4596 | 1379 | 319 | 231 |
| IF+OF1 \| OF2 \| EX+MA \| RW | 186 | 844 | 510 | 133 | 2130 | 467 | 577 | 102 | 4949 | 1430 | 405 | 196 |
| IF+OF1 \| OF2 \| EX \| MA+RW | 185 | 843 | 510 | 132 | 2137 | 466 | 577 | 101 | 4951 | 1430 | 405 | 196 |
| IF+OF1+OF2 \| EX \| MA \| RW | 207 | 1045 | 676 | 179 | 2555 | 557 | 739 | 123 | 6081 | 1450 | 521 | 131 |

## Decision: IF | OF | EX | MW (OF2 merged into EX, MA merged into RW)

| Stage | Was | Does |
|---|---|---|
| IF | IF | `mloadIR`, `madd pc, 4` |
| OF | OF1 | `mdecode`, read rs1 into `A` (register file read side 1) |
| EX | OF2 + EX | `mbeq I`, read rs2 / rd (st) / r15 (ret) (read side 2), `B` / `regData`, the ALU, flag branches, pc load |
| MW | MA + RW | load / store, write rd / r15 (write side), `<halt>` |

Section lengths in microcycles, in the order IF OF EX MW (six-stage in brackets,
IF OF1 OF2 EX MA RW):

| Instruction | 4-stage | 6-stage |
|---|---|---|
| `add r, r, r` | 2 3 3 2 | 2 3 3 1 0 2 |
| `add r, r, imm` | 2 3 2 2 | 2 3 2 1 0 2 |
| `mov r, imm` | 2 1 2 1 | 2 1 2 0 0 1 |
| `ld` | 2 3 1 3 | 2 3 1 1 1 2 |
| `st` | 2 3 2 2 | 2 3 2 1 2 0 |
| `bgt` (taken) | 2 1 2 0 | 2 1 0 2 0 0 |

Why it wins:

1. **The longest section stays 3 microcycles.** OF (`mdecode` + reading rs1) is 3
   and cannot be shortened. EX is also only 3, because `mmov B, regVal, <add>` is
   one microinstruction again. MW is at most 3 (`ld`).
2. **Far fewer stall beats (578 → 203).** Write-back is two stages after OF
   instead of four, so a dependent instruction waits at most 2 beats instead of 4.
3. **A smaller branch penalty (287 → 231 flushed).** EX is now the third stage,
   so a taken branch flushes 2 instructions instead of 3.

Why not the others:
- **Merging OF1 + OF2** gives a 6-microcycle OF section and doubles the beat.
- **Merging IF + OF1** gives a 5-microcycle stage. It also needs a hazard check
  on an instruction that is fetched in the same beat (the model allowed it, and
  this grouping still loses).
- **OF2+EX+MA | RW** comes a close second, but `st` takes 4 microcycles in one stage.

## Measured: the real four-stage CPU against the six-stage CPU

`risc_4stage_run` runs each program on all three processors and checks that they
end with the same registers, flags, memory and exception. Its microcycle counts
are exactly the model's predictions.

| Program | 4-stage µcycles | beats | µcycles/instr | stall beats | 6-stage µcycles | beats | µcycles/instr | stall beats | sequential | 4 vs 6 |
|---|---|---|---|---|---|---|---|---|---|---|
| factorial | 117 | 52 | 4.18 | 3 | 146 | 67 | 5.21 | 11 | 245 | 1.25× |
| sum_array | 545 | 208 | 4.29 | 40 | 671 | 293 | 5.28 | 104 | 1238 | 1.23× |
| fibonacci | 339 | 139 | 3.85 | 30 | 403 | 191 | 4.58 | 71 | 889 | 1.19× |
| gcd | 95 | 40 | 3.80 | 4 | 119 | 56 | 4.76 | 17 | 218 | 1.25× |
| bubble_sort | 1373 | 519 | 4.28 | 85 | 1670 | 732 | 5.20 | 262 | 3059 | 1.22× |
| recursive_factorial | 297 | 118 | 4.30 | 12 | 365 | 154 | 5.29 | 36 | 675 | 1.23× |
| popcount | 373 | 155 | 3.69 | 17 | 428 | 208 | 4.24 | 51 | 933 | 1.15× |
| hazards | 74 | 32 | 4.93 | 12 | 92 | 49 | 6.13 | 26 | 142 | 1.24× |
| **Total** | **3213** | | | **203** | **3894** | | | **578** | **7399** | **1.21×** |

`divide_by_zero.asm` raises DivideByZero at pc 0x0010 on all three processors
(17 microcycles on the four-stage, 23 on the six-stage).

A differential test of 1000 random programs also passed on all three processors:
220 of them end in an exception (173 misaligned accesses in MW, 25 divide by zero,
13 out of bounds, 9 writes to text).

## Findings

1. With the same microcode and no forwarding, the four-stage pipeline is about
   1.2× faster than the six-stage one, and about 2.3× faster than the sequential CPU.
2. The extra depth of the six-stage pipeline does not pay off: its stages are not
   shorter (the longest section is 3 microcycles in both), but its hazards and
   branches cost more beats.
3. The six-stage design would only win if its stages were shorter, e.g. if OF1's
   `mdecode` + rs1 read were split, or with forwarding, which removes most of the
   stall beats that a deeper pipeline suffers from.
