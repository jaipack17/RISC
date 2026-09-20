# Floating Point in RISC201 — Design Notes & Reading List

SimpleRisc is integer-only. Adding floating point is a legitimate "beyond
SimpleRisc" extension (per the assignment's item 1c/4 — "efficient
arithmetic... operations" and going beyond the base ISA), but it touches
every layer we've already started designing: encoding (§00 of the
microarchitecture doc), the register file, the ALU (§01c), the pipeline
depth debate (§01a), and the exception framework (assignment item 2f). This
doc is both the reading list and a first pass at the design questions, so we
have something concrete to argue about rather than starting from a blank
page.

## The four design questions, and a starting position on each

### 1. How do we store floating-point numbers?

Adopt **IEEE 754 binary32** ("single precision") as the format: 1 sign bit +
8 exponent bits (bias 127) + 23 fraction bits, implicit leading 1 for
normal numbers, fits exactly in a 32-bit RISC201 word. This isn't a hard
requirement — a teaching ISA could invent a simpler custom format — but
IEEE 754 is what every real RISC ISA converged on (MIPS, SPARC, PowerPC,
Alpha, RISC-V all use it), it's what lets us reuse decades of known hardware
algorithms, and it gives us a free correctness oracle: we can check RISC201
FP results against any language's native `float` type.

The one decision genuinely worth making ourselves, because real FPUs differ
on it: **what do we do with subnormal (denormal) numbers?** Full IEEE 754
compliance requires gradual underflow (denormals), which needs extra
normalizer logic for the smallest exponent range. Many embedded/teaching
FPUs instead **flush subnormals to zero** — simpler hardware, non-compliant
but defensible, and worth a paragraph in the report either way. Decide this
before writing the FPU's normalizer, not after.

### 2. Where do float values live — separate register file or shared?

Every real RISC ISA we surveyed (MIPS, RISC-V F/D, Alpha, SPARC, PowerPC)
uses a **separate floating-point register file**, not the integer GPRs.
Reasons that apply directly to RISC201:
- RISC201's 16 GPRs already have `r14=sp`, `r15=ra` reserved — a unified
  file makes float variables compete with pointers/stack for the remaining
  14 registers, which will hurt any program mixing pointer-heavy and
  float-heavy code (exactly the kind of program our stack/memory test suite
  will include).
- A separate file means FP loads/stores and FP arithmetic can be decoded and
  dispatched independently of the integer datapath — relevant for the
  microcoded core (item 3), where dispatch is already opcode-driven.
- The cost is real: extra register file, extra forwarding paths, and new
  `mov`-between-files instructions for conversions. That cost is the actual
  trade-off to discuss in the report, not just "we copied RISC-V."

Recommendation: add **8 dedicated FP registers `f0–f7`**, 32 bits each
(smaller than RISC-V's 32 to keep the encoding/regfile modest for a 4-stage
core), holding IEEE 754 binary32 values only — no double precision, to keep
scope bounded. `f0` need not be hardwired to zero (unlike `r0` conventions in
some ISAs) unless we want that for `fmov`-to-zero convenience.

### 3. How do we encode FP instructions without blowing up the opcode space?

We already planned RISC201's base opcode field as 5 bits (32 slots) with 21
SimpleRisc-derived instructions using ~21 of them (see the microarchitecture
doc, §00). Two options, mirroring real ISAs:

- **RISC-V's approach**: give floating point its own major opcode(s), then
  use a secondary `funct`-style field to select the specific FP operation
  (add/sub/mul/div/cmp/cvt/...) — this is exactly why RISC-V's `F` extension
  fits inside the same 32-bit instruction word as everything else without
  needing 15 separate major opcodes. We should do the same: reserve **one**
  RISC201 major opcode as "FP class", and pack a 3–4 bit `fp_funct` field
  plus the two/three `f`-register operands into the remaining bits.
- **MIPS's older approach**: a full separate "coprocessor" opcode namespace
  (coprocessor 1). More conceptually separate, but heavier — effectively a
  second decode table. Given we already need one shared `isa/table.py`
  source of truth (§00), the RISC-V-style single-major-opcode + funct-field
  approach is the better fit for RISC201: it's one more row-family in the
  same table, not a parallel ISA.

This is also where "optimally select encoding formats for different
instructions" (your original assembler-design question) comes back: FP
register-register ops (`fadd/fsub/fmul/fdiv/fcmp`) need 2–3 register fields
and no immediate — same shape as our integer R-format. FP load/store
(`fld/fst`) needs the same I-format (base register + offset) as integer
`ld/st`. So FP instructions mostly **reuse existing formats**, they don't
need new ones — the only new thing is the `f`-register field width (3 bits
for 8 registers) and the fp_funct sub-field.

### 4. How do FP instructions run alongside integer ones?

This is the pipeline-integration question, and it's the one with the
biggest knock-on effect on §01a of the microarchitecture doc.

- `fadd`/`fsub`/`fcmp`/`fmov`/`fabs`/`fneg` are (with a big enough EX stage
  or a 2-cycle EX) roughly as cheap as integer ops once the aligner and
  normalizer are built — budget them like the `mul`/`div` case we already
  flagged for the 6-stage core (variant **6F**, two-cycle EX).
- `fmul` and `fdiv` are strictly more expensive than integer `mul`/`div`:
  `fmul` still needs a full significand multiply (Booth radix-4 + Wallace
  tree — **the exact same hardware block we already planned for the integer
  ALU in §01c**, reusable almost as-is for the 24-bit significand multiply)
  plus exponent addition and a normalize/round pass. `fdiv` needs a
  significand divide (reuse the restoring/non-restoring/SRT divider from
  §01c, or implement Goldschmidt/Newton-Raphson as a genuinely different,
  comparable algorithm — see reading list below) plus exponent subtraction
  and rounding.
- Recommended scope decision: treat the whole FPU as a **multi-cycle
  functional unit that stalls the pipeline** while an `fmul`/`fdiv`/`fdiv`-class
  instruction is in flight, rather than building out-of-order completion
  with scoreboarding. This is a real simplification real FPUs don't make,
  but it's the correct scope call for a 4–5 person team also building two
  pipelines, a microcode engine, an assembler, and a debugger. State it
  explicitly as a scope decision in the report rather than silently doing
  it — this is precisely the kind of design trade-off the delay-model
  section of the microarchitecture doc argues you should always make
  explicit.
- IEEE 754 defines five standard exceptions: **invalid operation, division
  by zero, overflow, underflow, inexact**. These plug directly into the
  runtime exception framework the assignment already requires for stack
  safety (item 2f) — same vector/trap mechanism, different trigger
  conditions. Don't build a second exception system for FP; extend the one
  we're already building.

## Reading list

### Representation & correctness (read first)

- **IEEE 754 format itself** — read the single-precision layout and the
  special values (±0, ±∞, NaN, subnormals) before anything else:
  [IEEE 754 Floating-Point Format — IIT Kharagpur lecture notes](https://cse.iitkgp.ac.in/~goutam/pds/pdsLect/lect15.pdf)
- **The canonical correctness reference** — not just "what IEEE 754 is" but
  *why* it's designed the way it is (rounding modes, why guard/round/sticky
  bits exist, why NaN propagates):
  Goldberg, *What Every Computer Scientist Should Know About Floating-Point
  Arithmetic* — [full text (Sun/Oracle numerical guide)](https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html),
  [PDF copy](https://people.cs.pitt.edu/~cho/cs1541/current/handouts/goldberg.pdf).
  Read at least: representation, rounding error, and the IEEE standard
  section. This is the single most load-bearing reading-list item for the
  whole FP extension — it's what tells you which corner cases you're
  required to test (`cmp` on NaN, signed-zero, tie-to-even rounding).
- Original standard background, from the person who chaired the committee:
  [Kahan — IEEE Standard 754 for Binary Floating-Point Arithmetic](https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF)

### How real RISC ISAs expose floating point (ISA-level design)

- **RISC-V F/D extensions** — the most relevant reference again, because the
  spec explains rationale, not just encoding:
  [RISC-V "F" (single-precision) extension spec](https://docs.riscv.org/reference/isa/v20260120/unpriv/f-st-ext.html),
  [RISC-V "D" (double-precision) extension spec](https://docs.riscv.org/reference/isa/v20260120/unpriv/d-st-ext.html),
  and a readable secondary walk-through:
  [FPRox — "RISC-V and Floating-Point"](https://fprox.substack.com/p/risc-v-and-floating-point).
  Specifically read why they chose a **separate** register file (rationale
  section) and how NaN-boxing works if you're curious how RV32 handles
  double-precision in 32-bit registers — we're skipping double precision
  entirely, but it's useful context for why that's a real design cost.
- **MIPS coprocessor-1 model** — the historical "FP as a coprocessor" design,
  useful contrast to RISC-V's tighter integration:
  [MIPS32 Architecture manual, Vol. I (Cornell CS3410 mirror)](https://www.cs.cornell.edu/courses/cs3410/2008fa/MIPS_Vol1.pdf) —
  read the FPU/CP1 register and instruction-format sections.
- **bfloat16 / half precision**, if we ever want a "go further" footnote
  about reduced-precision formats (not recommended as core scope, but good
  report material for "why not 16-bit floats"):
  [Wikipedia — Bfloat16 floating-point format](https://en.wikipedia.org/wiki/Bfloat16_floating-point_format),
  [Nick Higham — "Half Precision Arithmetic: fp16 vs bfloat16"](https://nhigham.com/2018/12/03/half-precision-arithmetic-fp16-versus-bfloat16/).

### Hardware algorithms (what actually goes in the FPU)

- **FP adder/subtractor structure** (align → add/sub significands →
  normalize → round) — clear worked structural overview:
  [ScienceDirect Topics — Floating Point Addition](https://www.sciencedirect.com/topics/computer-science/floating-point-addition)
- **A genuinely hands-on hardware build log**, useful as a sanity check for
  logic-element/cycle-count budgeting (their multiplier: ~60 logic elements
  + 1 hardware multiplier @ ~50 MHz; their adder: ~220 logic elements @ ~30
  MHz on a Cyclone II — the adder being *slower* than the multiplier is a
  good talking point, since it's the align/normalize/round logic that
  dominates cost, not the arithmetic itself):
  [Bruce Land (Cornell ECE5760) — Floating Point Hardware](https://people.ece.cornell.edu/land/courses/ece5760/FloatingPoint/index.html)
- **FP multiplier**: exponents add (minus bias), significands multiply,
  signs XOR, then normalize/round. The significand multiply is exactly the
  Booth-radix-4 + Wallace-tree block already planned for the integer ALU
  (§01c of the microarchitecture doc) — same hardware, different operand
  width (24-bit significand incl. hidden bit vs. 32-bit integer).
- **FP divider algorithms** — two genuinely different families worth
  comparing for the assignment's "different ALU algorithms" evaluation item:
  - **Digit-recurrence** (restoring/non-restoring/SRT) — same family as the
    integer divider in §01c, applied to the significand.
  - **Goldschmidt / Newton-Raphson iterative reciprocal approximation** —
    quadratically-convergent, multiplier-based instead of subtractor-based,
    what most modern FPUs actually use:
    [Design and synthesis of a Goldschmidt-algorithm floating-point divider on FPGA](https://ieeexplore.ieee.org/document/7754360/)
    (if IEEE Xplore access isn't available, the Wikipedia articles on
    [Newton's method for reciprocals](https://en.wikipedia.org/wiki/Division_algorithm#Newton%E2%80%93Raphson_division)
    and Goldschmidt division give the algorithm outline).
  Comparing an SRT-style divider against a Goldschmidt/Newton-Raphson one is
  a strong, genuinely novel result for item 5 of the assignment (evaluating
  "different ALU algorithms") — better than just re-measuring the integer
  divider a second time.
- **Rounding hardware specifically** (guard/round/sticky bits, round-to-
  nearest-even) — this is the part that's easy to get subtly wrong and hard
  to unit-test without knowing what to check:
  search result summary and general G/R/S bit mechanics are covered
  alongside FMA design in modern FPU literature; the Goldberg paper above
  (rounding error section) is the more rigorous source — treat any
  patent-style hardware description as implementation inspiration only, not
  a citable source for the report.
- **Fused multiply-add (`fmadd`)**, optional "beyond SimpleRisc" instruction
  if we have time: computes `a*b+c` with a single rounding step instead of
  two, which is both a performance win and a *different, more accurate*
  numerical result than separate multiply+add — a good correctness talking
  point. Background on why single-rounding matters is in the Goldberg paper;
  RISC-V's `D`/`F` extension spec (above) documents `fmadd.s` as a concrete
  encoding example to model ours on.

## What to decide before writing `isa/table.py` entries for FP

1. Subnormals: full IEEE 754 gradual underflow, or flush-to-zero? (§1 above)
2. Register file: confirm `f0–f7`, 32-bit, single-precision only — or push
   back if the team wants double precision / a unified register file
   instead.
3. Opcode strategy: one FP major opcode + `fp_funct` field (recommended), or
   a MIPS-style separate coprocessor opcode space?
4. Pipeline scope: confirm "FPU stalls the pipeline" as the v1 scope
   decision, with scoreboarding/out-of-order FP as an explicitly-deferred
   stretch goal if time allows.
5. Instruction list to commit to for v1: `fadd fsub fmul fdiv fcmp fmov
   cvt.w.s cvt.s.w fld fst` at minimum, with `fabs fneg fmadd` as stretch
   instructions.
