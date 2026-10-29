# Six-stage pipeline: choosing the stage split (experiment, 2026-10-29)

The six-stage pipelined microprogrammed processor runs `microcode/vertical.masm`
unchanged. The splitter (`src/microassembler/splitter.cpp`) gives each stage its
section of every instruction's routine. A beat lasts as long as the longest
section in it, so how the work is shared out between the stages decides the speed.

With the first split (IF, OF, ALU, BR, MA, RW), OF was by far the longest stage
(6 microcycles for `add r, r, r`). Two ways of shortening it were compared:

| Variant | Stages | Change |
|---|---|---|
| A: current split | IF, OF, ALU, BR, MA, RW | none |
| B: option 1 | IF, OF, ALU, BR, MA, RW | a line that moves into `A`/`B` and has ALU args (e.g. `mmov B, regVal, <add>`) runs entirely in the ALU stage instead of being split OF + ALU |
| C: option 3 | IF, **OF1, OF2, EX**, MA, RW | OF is split in two: OF1 = `mdecode` + read rs1 into `A`; OF2 = `mbeq I` + read rs2 / rd (st) / r15 (ret) + `B ←` / `regData ←`. BR is merged into the ALU stage (EX). |
| D: options 1 + 3 | IF, OF1, OF2, EX, MA, RW | both |

## Method

These numbers come from a **timing model**, not from the pipelined CPU (which
was not built yet):

1. Each program is run on the hardwired CPU to get the exact sequence of executed
   instructions (and which branches were taken).
2. For each variant, the real splitter code (with that variant's stage rules)
   builds the six stage microprograms from `vertical.masm`.
3. Each instruction's section length in each stage is found by walking its stage
   microprogram (using its I bit and branch outcome).
4. The pipeline is stepped beat by beat. Beat length = the longest section among
   the stages that run in that beat (at least 1 microcycle).

Rules used for **every** variant:

- Data hazards: an instruction waits in the stage that reads a register until the
  instruction writing that register has left RW (no forwarding). IF and the
  stages before it hold; a bubble enters the next stage.
- Taken branches and `hlt` are resolved in stage 3 (BR or EX). The younger
  instructions are flushed. Wrong-path instructions are fetched and occupy
  stages until then.
- The flags (`cmp` → `beq`) are not treated as a hazard.

The absolute numbers will change once the hazard rules are final (step 7), but
the rules are the same for all four variants, so the comparison between them is fair.

## Programs

- `programs/factorial.asm`: 5! (28 instructions executed)
- sumloop: fills an array with 0..9 at 0x4000, then sums it (117 instructions)
- demo: `mov r1, 5; mov r2, 0x4000; add r3, r1, r1; st r3, 0[r2]; hlt` (5 instructions)
- independent: 8 `mov`s, then 8 ALU instructions with no neighbouring dependencies, then `hlt` (17 instructions)

## Results

### Total microcycles (fewer is better)

| Program (instructions executed) | Sequential micro CPU (measured) | A: current split | B: option 1 | C: option 3 | D: options 1 + 3 |
|---|---|---|---|---|---|
| factorial (28) | 245 | 205 | 184 | 149 | 148 |
| sumloop (117) | 1181 | 898 | 809 | 627 | 616 |
| demo (5) | 45 | 39 | 37 | 30 | 30 |
| independent (17) | 169 | 86 | 78 | 53 | 52 |

### Microcycles per instruction

| Program | A | B | C | D |
|---|---|---|---|---|
| factorial | 7.32 | 6.57 | 5.32 | 5.29 |
| sumloop | 7.68 | 6.91 | 5.36 | 5.26 |
| demo | 7.80 | 7.40 | 6.00 | 6.00 |
| independent | 5.06 | 4.59 | 3.12 | 3.06 |

### Beats, stall beats and flushed instructions

| Program | Variant | Beats | Stall beats | Flushed |
|---|---|---|---|---|
| factorial | A, B | 67 | 12 | 25 |
| factorial | C, D | 67 | 12 | 25 |
| sumloop | A, B | 301 | 126 | 56 |
| sumloop | C, D | 270 | 95 | 56 |
| demo | A, B | 17 | 8 | 2 |
| demo | C, D | 16 | 7 | 2 |
| independent | A, B | 23 | 2 | 2 |
| independent | C, D | 22 | 1 | 2 |

### Section lengths in microcycles

Columns are the six stages in order: IF, OF / OF1, ALU / OF2, BR / EX, MA, RW.

| Instruction | A: current | B: option 1 | C: option 3 | D: options 1 + 3 |
|---|---|---|---|---|
| `add r, r, r` | 2 6 1 0 0 2 | 2 5 2 0 0 2 | 2 3 3 1 0 2 | 2 3 2 2 0 2 |
| `add r, r, imm` | 2 5 1 0 0 2 | 2 4 2 0 0 2 | 2 3 2 1 0 2 | 2 3 1 2 0 2 |
| `mov r, imm` | 2 3 0 0 0 1 | 2 3 0 0 0 1 | 2 1 2 0 0 1 | 2 1 2 0 0 1 |
| `ld` | 2 4 1 0 1 2 | 2 3 1 0 1 2 | 2 3 1 1 1 2 | 2 3 0 1 1 2 |
| `st` | 2 5 1 0 2 0 | 2 4 1 0 2 0 | 2 3 2 1 2 0 | 2 3 1 1 2 0 |
| `cmp r, r` | 2 6 1 0 0 0 | 2 5 2 0 0 0 | 2 3 3 1 0 0 | 2 3 2 2 0 0 |
| `bgt` (taken) | 2 1 0 2 0 0 | 2 1 0 2 0 0 | 2 1 0 2 0 0 | 2 1 0 2 0 0 |

## Findings

1. Option 1 alone saves about 10%. OF is still the bottleneck (5).
2. Option 3 saves about 27–38%. The longest section drops from 6 to 3, and there
   are fewer stall beats because rs2 is read one stage later. The branch penalty
   is unchanged (the decision is still in stage 3).
3. Option 1 on top of option 3 adds almost nothing: after option 3 the
   bottleneck is OF1 (`mdecode` + reading rs1), which option 1 does not shorten.
4. Against the sequential microprogrammed CPU, variant D runs factorial in 148
   instead of 245 microcycles, and sumloop in 616 instead of 1181 (about 1.6–1.9×).
