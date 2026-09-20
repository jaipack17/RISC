# Reading List: Designing the RISC201 ISA

Purpose: background reading before we lock the RISC201 instruction set. Organized
in tiers — read Tier 1 first, it's enough to start designing; Tier 2/3 are for
when we want to justify a specific decision (why this addressing mode, why
this encoding, why add this instruction) or go beyond SimpleRisc.

## Tier 0 — what we already have

- Sarangi, *Basic Computer Architecture* — the SimpleRisc ISA chapter (assembly
  language / ISA chapter, and the chapter introducing ARM & x86 for contrast).
  This is our baseline: 21ish instructions, 16 registers, fixed 32-bit
  encoding, 3 instruction formats, 2-bit flags register. Re-read the exact
  opcode table and instruction formats before designing RISC201 — we are
  diverging from it deliberately, so we need to know precisely what we're
  diverging from.
- Course lecture referencing SimpleRisc directly:
  [Computer Architecture (Prof. Smruti R. Sarangi, IIT Delhi) — Lecture 06: Assembly Language: SimpleRisc ISA](http://www.infocobuild.com/education/audio-video-courses/computer-science/ComputerArchitecture-Sarangi-IIT-Delhi/lecture-06.html)

## Tier 1 — core ISA design principles (read these first)

1. **Patterson & Hennessy, "Instruction Set Principles"** — the standard
   textbook treatment of what an ISA is made of: operand storage
   (stack/accumulator/register), addressing modes, operation types, encoding
   trade-offs. Two good open copies of essentially this chapter:
   - [Swarthmore CS31 — Ch. 2, Instruction Set Architecture](https://www.cs.swarthmore.edu/~richardw/classes/cs31/s18/resources/0073380652_Ch2.pdf)
   - [WSU EE524/CptS561 — Chapter 2 slides, Instruction Set Principles](https://eecs.wsu.edu/~jdelgado/CompArch/Chapter2.prn.pdf)
2. **Addressing modes**, focused treatment:
   - [WITScad — ISA: Addressing Modes](https://witscad.com/course/computer-architecture/chapter/isa-addressing-modes)
   - [Addressing Modes and Formats (Ch. 11 notes)](http://aturing.umcs.maine.edu/~meadow/courses/cos335/COA11.pdf)
3. **The RISC vs. CISC case** — this is the argument SimpleRisc and RISC201 are
   both descendants of. Read the original, not just a summary:
   - Patterson & Ditzel, *The Case for the Reduced Instruction Set Computer*
     (1980): [UT Austin copy](https://www.cs.utexas.edu/~fussell/courses/cs352h/papers/risc.pdf)
   - Patterson & Séquin, *RISC I: A Reduced Instruction Set VLSI Computer*
     (ISCA 1981) — the actual chip that followed from the manifesto above:
     [Berkeley copy](https://people.eecs.berkeley.edu/~kubitron/courses/cs252-F00/handouts/papers/p216-patterson.pdf)
4. **MIPS as a second reference point** (Sarangi's own SimpleRisc is
   MIPS-flavored). The MIPS "green sheet" is the best single-page instruction
   reference to skim for "what categories of instruction does a clean RISC ISA
   actually need":
   [MIPS Reference Data / Green Sheet (UC Berkeley CS61C)](https://inst.eecs.berkeley.edu/~cs61c/resources/MIPS_Green_Sheet.pdf)

## Tier 2 — a modern, actively-designed ISA to calibrate against

RISC-V is the most useful modern reference because it's a clean-slate RISC
design with public design-rationale documents — exactly the kind of decisions
we'll be making for RISC201.

1. **Why a new clean ISA at all** — same category of exercise we're doing,
   at a different scale:
   Asanović & Patterson, *Instruction Sets Should Be Free: The Case for RISC-V*
   (UCB/EECS-2014-146): [PDF](http://www2.eecs.berkeley.edu/Pubs/TechRpts/2014/EECS-2014-146.pdf)
2. **The spec itself**, for its base instruction categories and its
   *rationale* callouts (the spec explains *why* each encoding choice was
   made, which is more useful to us than the encoding itself):
   [RISC-V Instruction Set Manual, Vol. I: Unprivileged Architecture](https://docs.riscv.org/reference/isa/_attachments/riscv-unprivileged.pdf)
   — read at least: base integer instruction categories (RV32I), the M
   extension (multiply/divide), and skim the rationale sidebars.
3. **Compressed / variable-length encoding rationale** — relevant to us
   because "optimally select instruction encoding formats for different
   instructions" (your ask) is precisely the problem RVC solves:
   [RISC-V "C" Compressed Instructions spec](https://docs.riscv.org/reference/isa/unpriv/c-st-ext.html) —
   note in particular how they pick *which* instructions get a compressed
   form (the frequent ones: register-register ops, small immediates, sp-relative
   loads/stores) and how they steal encoding space by disallowing rare
   register/immediate combinations (e.g., `x0`, zero immediates).
4. **Going beyond a minimal integer ISA** — read these for instruction
   *categories* to consider adding, even if we only borrow 2–3 instructions
   from each:
   - Bit-manipulation (Zbb: population count, count-leading/trailing-zeros,
     min/max, byte/bit rotate, sign/zero-extend helpers):
     [RISC-V Bit-Manipulation ISA-extensions spec](https://www.ece.lsu.edu/ee4720/doc/riscv-bitmanip-1.0.0.pdf)
   - Atomics, if we want a `lr`/`sc` or `cas`-style instruction for the
     stack-safety / exception story: see Tier 3 below.

## Tier 2b — real-world RISC ISA specifications (survey)

You asked specifically for actual shipped RISC ISAs with their real
specifications, not just RISC-V/MIPS/ARM. Here's a broader survey — same
lineage (Berkeley RISC / Stanford MIPS → commercial RISC ISAs of the
1980s–2020s), each with the primary spec document. Read the *instruction
formats / addressing modes* chapter of each, not the whole manual.

| ISA | Origin | Primary spec | What's distinctive about it |
|---|---|---|---|
| **MIPS** | Stanford RISC project (Hennessy, 1981) → commercial MIPS | [MIPS Green Sheet / Reference Data](https://inst.eecs.berkeley.edu/~cs61c/resources/MIPS_Green_Sheet.pdf) | The closest relative of SimpleRisc — fixed 32-bit encoding, 3 instruction formats (R/I/J), delayed branch slot (original MIPS), no condition-code register (uses register-register `slt`-style compare). Good baseline to diff RISC201 against. |
| **ARM (AArch64/A64)** | Acorn RISC Machine, 1985 → ARMv8-A | [ARMv8-A Instruction Set Overview](https://class.ece.uw.edu/469/peckol/doc/ARM/ARM_v8_Instruction_Set_Architecture_(Overview).pdf) | Fixed 32-bit encoding but far richer addressing modes than MIPS — pre/post-increment load/store (maps directly onto stack push/pop), shifted-register operands folded into ALU ops, conditional execution via a flags register. Worth reading for the load/store addressing-mode design specifically. |
| **RISC-V (RV32I)** | UC Berkeley, 2010 | [RISC-V Unprivileged ISA Manual](https://docs.riscv.org/reference/isa/_attachments/riscv-unprivileged.pdf) | Already in Tier 2 — the only one of these with public, instruction-by-instruction *design rationale* text, which is why it's still the main reference. |
| **SPARC** | Berkeley RISC (Patterson/Séquin) → Sun Microsystems, 1987 | [SPARC Architecture Manual, Version 8](https://courses.grainger.illinois.edu/cs423/sp2011/lectures/sim_public/sparcv8.pdf) · [quick instruction-set guide](https://cseweb.ucsd.edu/~ricko/CSE131/sparc_guide.pdf) | Direct commercial descendant of the *Berkeley RISC I* design from the original 1981 paper — register windows for fast procedure calls (a whole different answer to the "calling convention" question in Tier 3), annulled delay-slot branches. |
| **Power ISA / PowerPC** | IBM 801 lineage → Apple–IBM–Motorola alliance, 1991 | [PowerPC User ISA Book I, v2.01](https://math-atlas.sourceforge.net/devel/assembly/ppc_isa.pdf) | Fixed 32-bit encoding but with a much larger, more "batteries included" instruction count than MIPS/RISC-V — condition-register *fields* (8 separate 4-bit condition fields, not one flags register) and fused compare-to-field instructions. Useful counterexample if we're weighing "how many flags/condition bits" for RISC201. |
| **Alpha AXP** | DEC, 1992 (designed by Sites & Witek) | [Alpha AXP Architecture Reference Manual](https://www.bitsavers.org/pdf/dec/alpha/Sites_AlphaAXPArchitectureReferenceManual_2ed_1995.pdf) · [short abstract](https://danluu.com/dick-sites-alpha-axp-architecture.pdf) | The most aggressively "pure RISC" of the bunch: **no condition-code/flags register at all** (every compare writes its result into a general register), no byte load/store in the base ISA (synthesized from word load + shift/mask). Directly informs the Tier-3 condition-codes decision by showing the opposite extreme from SimpleRisc's flags register. |
| **AVR** | Atmel, 1996 | [AVR Instruction Set Manual](https://ww1.microchip.com/downloads/en/DeviceDoc/AVR-Instruction-Set-Manual-DS40002198A.pdf) | An 8-bit *microcontroller*-class RISC ISA — useful as the small/embedded end of the spectrum: still load-store/register-register in spirit, but with a flags register (`SREG`) closer to SimpleRisc's model than Alpha's, and single-cycle-focused instruction selection. Good contrast if RISC201 ever gets an embedded/low-resource discussion in the report. |

### Design retrospectives / blogs on these ISAs

Narrative, non-spec sources that explain *why* these architectures made the
choices they did — useful for the report's motivation section, and often
more readable than the manuals:

- [The Chip Letter — "The RISC Wars, Part 2: The Age of Alliances"](https://thechipletter.substack.com/p/the-risc-wars-part-2-the-age-of-alliances) —
  MIPS vs. SPARC vs. PA-RISC vs. POWER/PowerPC vs. Alpha, told as a
  business-and-design history; good for understanding *why* these ISAs
  diverged from each other despite a shared starting philosophy.
- [Hackaday — "History of the SPARC CPU Architecture"](https://hackaday.com/2023/03/28/history-of-the-sparc-cpu-architecture/) —
  short, accessible, traces SPARC directly back to the Berkeley RISC I paper
  in Tier 1.
- [Computer History Museum — "RISC: Is Simpler Better?"](https://www.computerhistory.org/revolution/digital-logic/12/286) —
  short museum piece on the original RISC-vs-CISC debate, good as a two-minute
  primer before reading the Patterson & Ditzel paper itself.
- [EPCC — "The Evolution of RISC Architecture"](https://www.epcc.ed.ac.uk/whats-happening/articles/evolution-risc-architecture) —
  traces the line from Berkeley RISC/Stanford MIPS through to RISC-V and
  today's Arm dominance in mobile/embedded.
- [Lighterra — "Modern Microprocessors: A 90-Minute Guide!"](https://www.lighterra.com/papers/modernmicroprocessors/) —
  not RISC-specific, but the clearest free explanation of *why* instruction
  set and pipeline decisions interact (branch penalties, superscalar issue,
  etc.) — useful once we're cross-referencing ISA choices against the
  4-stage/6-stage pipeline comparison in the microarchitecture doc.

## Tier 3 — specific design decisions relevant to RISC201

- **Condition codes vs. compare-and-branch.** SimpleRisc uses a flags
  register (`cmp` sets flags, `beq`/`bgt`/... read them). This is a real
  design fork with known trade-offs (flags create a hidden dependency that
  hurts pipelining/scheduling; fused compare-and-branch avoids that but
  costs encoding bits). Good overview: [Mark Smotherman — Condition Codes](https://people.computing.clemson.edu/~mark/conditioncodes.html).
  Decide *now* whether RISC201 keeps SimpleRisc's flags register, moves to
  RISC-V-style register-register compare-and-branch, or supports both — this
  choice affects the pipeline's forwarding logic (see the microarchitecture
  doc, §01a) and the ALU's flag-computation correctness (§01c).
- **Calling convention / ABI**, needed for item 2e/2f in the assignment
  (stack macro preprocessor, full-descending stack, stack-boundary safety).
  We need to decide the register convention (which regs are caller-saved vs
  callee-saved, which reg is `sp`, which is `ra`) before the preprocessor can
  expand `PUSH`/`POP`/`CALL`/`RET` macros correctly.
  - [CMU 15-411 — Lecture Notes on Calling Conventions](https://www.cs.cmu.edu/~janh/courses/411/23/lec/11-calling.pdf)
  - [RISC-V ELF psABI — Calling Convention doc](https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-cc.adoc)
    (a real-world example of exactly the document we need to write for
    RISC201: which regs are argument/return/saved/temp)
- **Atomics (optional extension idea)**, if we want RISC201 to have a
  synchronization primitive beyond the scope of SimpleRisc: LL/SC fits a
  load-store RISC ISA more naturally than CAS (2-register operands, separate
  read/write instructions, fits the pipeline model). Background:
  [CAS, ABA and LL/SC](https://blog.memzero.de/cas-llsc-aba/),
  [Wikipedia — Load-link/store-conditional](https://en.wikipedia.org/wiki/Load-link/store-conditional).
- **ARM as a third data point**, useful specifically for its addressing-mode
  richness (pre/post-increment load/store, which maps well onto our
  full-descending stack push/pop) and its use of a fixed 32-bit encoding with
  a much larger instruction count than SimpleRisc — a useful ceiling to look
  at and then deliberately not fully copy:
  [ARMv8-A Instruction Set Overview](https://class.ece.uw.edu/469/peckol/doc/ARM/ARM_v8_Instruction_Set_Architecture_(Overview).pdf).

## What "instruction categories to cover" should shake out to

Cross-referencing SimpleRisc, MIPS, and RISC-V base+extensions, a complete
teaching RISC ISA needs at least:

| Category | Examples | Notes for RISC201 |
|---|---|---|
| Arithmetic | `add sub mul div mod` (+ maybe `neg`) | `mul/div/mod` are the ones that won't fit in 1 ALU cycle — feeds directly into the 6-stage/microcode ALU design |
| Logical | `and or xor not` | straightforward, single ALU cycle |
| Shift/bit | `lsl lsr asr`, optionally `rol/ror`, `clz/ctz`, `popcount` | shifter is already required (§01c); bit-manipulation ops are the "beyond SimpleRisc" opportunity |
| Comparison | `cmp` (flag-setting) or `slt`-style register compare | pick one model, see Tier 3 above |
| Data movement | `mov`, `movh`/`lui`-style upper-immediate load | needed to build 32-bit constants from a narrow immediate field |
| Memory | `ld st` with at least register+offset addressing; consider pre/post-increment for stack ops | addressing mode choice directly drives the stack push/pop macro design (item 2e) |
| Control flow | unconditional `b`/`j`, conditional `beq/bgt/...`, `call`/`ret` (or `jal`-style link) | `call/ret` needs a defined link register — ties to ABI decision above |
| Stack | `push pop` (macro-expanded, not necessarily native opcodes) | item 2e explicitly asks for a stack preprocessor — decide if these are real instructions or pure macros over `ld/st` + `sp` update |
| System/misc | `nop`, `halt`/`trap`, exception/interrupt entry | `nop` is trivial but required for pipeline hazard testing; trap/exception ties to item 2f (runtime exception framework) |
| Beyond-SimpleRisc candidates | `min/max`, conditional-move/select, sign/zero-extend, `lr/sc` atomics, `clz/popcount` | pick a small, justifiable set — don't add instructions we can't also implement in the ALU and microcode |

## Suggested reading order

1. Re-read the SimpleRisc chapter in Sarangi (Tier 0) with the opcode table
   open — write down every instruction and its format.
2. Patterson & Hennessy ISA-principles chapter + addressing-modes notes
   (Tier 1, items 1–2) — this gives us the vocabulary (operand storage class,
   addressing mode, operation type) to talk about design choices precisely.
3. Skim RISC-V base ISA + rationale (Tier 2, item 2) specifically for
   instructions SimpleRisc lacks.
4. Read the condition-codes page and the calling-convention notes (Tier 3) —
   these are the two decisions that most affect other parts of the project
   (pipeline forwarding, stack preprocessor) and are cheapest to get right
   before we start encoding.
5. Everything else in Tier 2/3 is optional depth — pull in only the sections
   relevant to whichever "beyond SimpleRisc" instructions we decide to add.
