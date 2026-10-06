# RISC201 Microassembly (`.masm`) — vertical microcode

Microcode for the microprogrammed RISC201 processor is written as text in a `.masm`
file. The microassembler turns it into the contents of the microprogram memory
(45-bit vertical microinstructions, Figure 9.24) and the switch table used by
`mswitch`. The language follows Sarangi, *Basic Computer Architecture*, §9.6
(Table 9.8) and §9.7.

## 1. Lines

Each line is one of: blank, a comment only, a label, a microinstruction, or a label
followed by a microinstruction on the same line.

```asm
@ a comment
.begin:                         @ a label alone
    mloadIR                     @ a microinstruction
.add_rw: mmov regSrc, rd        @ a label + a microinstruction
```

- **Comments** start with `@` and run to the end of the line (the same as RISC201
  program assembly).
- Indentation and extra spaces do not matter. Operands are separated by commas.
- **Case does not matter** for microinstructions, registers, args, and labels:
  `mloadIR` = `MLOADIR`, `regSrc` = `regsrc`.

## 2. Labels

```asm
.name:
```

- A dot, then a letter or `_`, then letters, digits, or `_`. Defined with a trailing
  `:`, used without it (`mb .begin`).
- A label is the **address of the next microinstruction**, counted in
  microinstructions (0, 1, 2, ...), not bytes.
- **Entry labels.** If the name without the dot is one of the RISC201 instruction
  mnemonics (`.add`, `.ld`, `.beq`, `.hlt`, ...), it is also **that opcode's entry in
  the switch table**: `mswitch` jumps there.
- **Internal labels.** Any other name (`.begin`, `.add_imm`, `.beq_branch`) is only a
  jump target. Prefix internal labels with their routine's name to avoid clashing
  with a mnemonic.
- A label may be defined only once.

## 3. Microinstructions (Table 9.8)

| Syntax | Meaning |
|---|---|
| `mloadIR` | `ir ← [pc]` |
| `mdecode` | fill the decode registers (`I`, `rd`, `rs1`, `rs2`, `immx`, `branchTarget`) from `ir` |
| `mswitch` | `µpc ← switch table[opcode in ir]` |
| `mmov dest, src` / `mmov dest, src, <args>` | `dest ← src`, then send `args` |
| `mmovi dest, imm` / `mmovi dest, imm, <args>` | `dest ← imm`, then send `args` |
| `madd reg, imm` / `madd reg, imm, <args>` | `reg ← reg + imm` (µadder), then send `args` |
| `mbeq reg, imm, .label` | if `reg == imm` then `µpc ← .label` |
| `mb .label` | `µpc ← .label` |

The destination comes first, as in the book (`mmov reg1, reg2` means `reg1 ← reg2`).
When a microinstruction has `<args>`, the value moves over the bus first and the
unit then performs the operation in the same cycle.

## 4. Registers (Table 9.7)

| Usable as | Registers |
|---|---|
| **source** (`src` of `mmov`, `reg` of `mbeq`) | `pc`, `I`, `rd`, `rs1`, `rs2`, `immx`, `branchTarget`, `regSrc`, `regData`, `regVal`, `A`, `B`, `flagE`, `flagGT`, `aluResult`, `mar`, `mdr`, `ldResult` |
| **destination** (`dest` of `mmov`/`mmovi`, `reg` of `madd`) | `pc`, `regSrc`, `regData`, `regVal`, `A`, `B`, `aluResult`, `mar`, `mdr`, `ldResult` |
| **neither** | `ir` (not connected to the bus) |

The decode registers and the flags only drive the write bus, so they cannot be
destinations (§9.7).

## 5. Immediates

Decimal (`4`, `-4`, `15`) or hexadecimal (`0x7FF`, `-0x10`), in the 12-bit signed
range **-2048 to 2047**.

## 6. Args (§9.7.2)

| Unit | Args |
|---|---|
| ALU | `<add>` `<sub>` `<cmp>` `<mul>` `<div>` `<mod>` `<lsl>` `<lsr>` `<asr>` `<and>` `<or>` `<xor>` `<not>` |
| Register file | `<read>` `<write>` |
| Memory unit | `<load>` `<store>` |
| µcontrol unit | `<halt>` |

Args are the optional last operand of `mmov`, `mmovi`, and `madd` only.

## 7. Execution starts at µaddress 0

The µpc resets to 0, so the first microinstruction in the file runs first. By
convention that is the preamble (§9.6.3):

```asm
.begin:
    mloadIR
    mdecode
    madd    pc, 4
    mswitch
```

## 8. Halting

```asm
.hlt:
    madd    pc, -4, <halt>      @ pc back on the hlt instruction, then stop
```

The preamble already advanced `pc`, so subtracting 4 leaves `pc` pointing at the
`hlt` instruction (the same as the hardwired processor), and `<halt>` stops the
µcontrol unit.

## 9. Errors and warnings

Errors report the source line.

- Unknown microinstruction, register, or args.
- Wrong number or kind of operands.
- A register used in a direction it is not connected for, or `ir` used at all.
- Immediate outside -2048..2047.
- `<args>` on a microinstruction that does not take them.
- Bad label name, label defined twice, or label used but never defined.
- More than 1024 microinstructions.
- **Warning:** an opcode with no entry label (for example `bc` and `bv`, which need
  the C/V flags). Executing such an opcode raises `IllegalInstruction`.

## 10. Output

1. The microprogram memory contents: one 45-bit word per microinstruction, word *i*
   at µaddress *i*.
2. The switch table: `opcode → µaddress` for every entry label.
3. Optionally a listing: µaddress, word in hex, and source line.

## Example

```asm
@ microcode/vertical.masm

.begin:
    mloadIR
    mdecode
    madd    pc, 4
    mswitch

.add:
    mmov    regSrc, rs1, <read>
    mmov    A, regVal
    mbeq    I, 1, .add_imm
    mmov    regSrc, rs2, <read>
    mmov    B, regVal, <add>
    mb      .add_rw
.add_imm:
    mmov    B, immx, <add>
.add_rw:
    mmov    regSrc, rd
    mmov    regData, aluResult, <write>
    mb      .begin

.b:
    mmov    pc, branchTarget
    mb      .begin

.hlt:
    madd    pc, -4, <halt>
```

Labels resolve to `.begin` = 0, `.add` = 4, `.add_imm` = 10, `.add_rw` = 11,
`.b` = 14, `.hlt` = 16. The switch table gets ADD → 4, B → 14, HLT → 16.
