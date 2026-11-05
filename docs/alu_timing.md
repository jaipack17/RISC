# ALU algorithms: counted delays and their effect on the pipelines

## What changed

Sarthak's ALU suite (branch `alu-optimization`) had two gaps:
- Every algorithm finished in one µcycle, so choosing one changed no timing.
- The delay, gate and FO4 figures were typed in.

Now:
- **Every unit is built from gates while it computes** (`include/risc/alu/gates.hpp`). Each wire is
  a `Bit` with a value and the time it settles. Each gate adds its delay and area, so the delay is
  counted from the same code that gives the answer.
- **An ALU operation that needs more than one µcycle holds its stage** (fixed clock, multi-cycle
  ALU). The µcontrol unit keeps its µpc on the ALU microinstruction until the ALU is done. In the
  pipelines this lengthens the EX section, and so the beat. The same happens in the sequential
  microprogrammed CPU.

## The model

**Unit-gate model:**

| Gate | Delay | Area |
|---|---|---|
| 2-input AND / OR | 1 | 1 |
| XOR | 2 | 2 |
| 2:1 mux | 2 | 3 |
| inverter | 0 | 0 |

Wide ANDs and ORs are built as balanced trees.

**Steps.** A multiplier or divider does one step per clock. Each step makes at most one pass
through the adder. A step's results go into registers (`Circuit::clock()`), and it takes
`max(1, ceil(step delay ÷ µcycle))` µcycles.

**The µcycle** is 22 gate delays. That is the longest single step of any operation when the
fastest adder (Kogge-Stone) is used, so with it every ALU operation and every multiplier or
divider step fits in one µcycle. The length is measured, not chosen (`measure_microcycle_length()`).

**ALU paths.**
- An add or sub passes b through XORs, which is how it subtracts, then the adder.
- A cmp also needs the flags: E comes from an XOR per bit and a NOR tree, beside the adder.
  GT = !E && (N == V).

**Defaults** (the fastest of each): Kogge-Stone adder, Booth radix-4 multiplier, non-restoring
divider.

## Counted figures (`risc_alu_test`)

**Adders:**

| Adder | Delay | Area | cmp delay | add µcycles | cmp µcycles |
|---|---|---|---|---|---|
| Ripple-carry | 66 | 224 | 74 | 4 | 4 |
| Carry-select (blocks of 4) | 24 | 441 | 32 | 2 | 2 |
| Hierarchical CLA (groups of 4, then groups of groups) | 21 | 587 | 29 | 2 | 2 |
| Kogge-Stone prefix tree | 14 | 611 | 22 | 1 | 1 |

**Multipliers and dividers with the Kogge-Stone adder:**

| Unit | Average µcycles per operation (range) |
|---|---|
| Shift-and-add (magnitudes) | 33.4 (32–34) |
| Booth radix-2 | 32.0 |
| Booth radix-4 | 16.0 |
| Restoring | 64.5 (33–67) |
| Non-restoring | 34.5 (32–36) |

- The averages are over edge-case and random operand pairs.
- A slower adder makes every step longer: with ripple-carry, Booth radix-4 averages 44.5
  µcycles and restoring division 258.
- Restoring division needs an extra step whenever it adds the divisor back.
- Shift-and-add needs extra steps to negate negative operands and the product.

## On the 100 benchmarks (total µcycles, one unit changed at a time)

| ALU | 4-stage | 6-stage | sequential | 6 ÷ 4 |
|---|---|---|---|---|
| default | 234,906 | 284,109 | 457,525 | 1.21× |
| ripple-carry adder | 350,809 (1.49×) | 385,783 | 583,735 | 1.10× |
| carry-select or CLA adder | 267,923 (1.14×) | 306,895 | 499,595 | 1.15× |
| shift-and-add or Booth radix-2 | 252,860 (1.08×) | 302,063 | 475,479 | 1.19× |
| restoring divider | 244,799 (1.04×) | 294,002 | 467,418 | 1.20× |

- **The adder matters most.** It is used by add, sub, cmp and every ld/st address.
- **The multiplier and divider only affect the programs that multiply (41) or divide (14).** On
  "Divide and modulo", the ripple-carry adder makes the 4-stage pipeline 3.55× slower.
- **The 4-stage pipeline stays faster with every ALU.** Its lead shrinks with slow adders,
  because both pipelines pay the extra µcycles.
- **The single-cycle-ALU numbers in `four_stage_split_experiments.md` no longer hold.** These
  replace them: multiply and divide now take 16 and about 34 µcycles instead of 1.

## Verification

- Every adder, multiplier and divider was checked with every adder against C++ arithmetic: about
  2.1 million checks, 0 wrong.
- 2,004 random programs (including stack faults and divide by zero) end in the same state on the
  4-stage, 6-stage and sequential CPUs under four ALU configurations.
- All 100 benchmarks agree under all 7 configurations the ALU page runs.

## Where to look

- **Command line:**
  - `risc_alu_test` prints the counted tables.
  - `risc_4stage_run` and `risc_6stage_run` take `--adder ripple|select|cla|prefix`,
    `--mul shiftadd|booth2|booth4` and `--div restoring|nonrestoring`.
- **Web app:**
  - The **ALU algorithms** page.
  - The Simulator's ALU dropdowns. EX shows "ALU busy, n more" while it waits.
