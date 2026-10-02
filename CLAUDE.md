# RISC201 Project Guide

## Current status

This is a C++20 RISC201 simulator with a hardwired five-unit baseline and a two-pass assembler.

The CPU is organized as:

```text
IF -> OF -> EX -> MA -> RW
```

`CPU::step()` currently executes those units sequentially for one instruction and records a `CycleTrace` for the terminal visualizer. It is not yet an overlapping, clock-accurate pipeline with pipeline registers, forwarding, hazards, or stalls. Complete and validate this hardwired baseline before implementing the assignment's 4-stage, 6-stage, microprogrammed, stack, debugger, and evaluation extensions.

## Build and run

The current `build/` directory uses MinGW Makefiles, so executables appear directly under `build/`.

From the repository root in CMD:

```cmd
cmake --build build
```

If there is no configured build directory:

```cmd
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Run the built-in hardcoded demo:

```cmd
build\risc_run.exe 100
```

Run an assembly source file through the assembler and CPU:

```cmd
build\risc_run.exe programs\sum.asm 100
```

The final number is the visualizer delay in milliseconds per simulated CPU step. Use `0` for no intentional delay.

Create a standalone hexadecimal text image:

```cmd
build\risc_asm.exe programs\sum.asm programs\sum.hex
```

## Current architecture

### ISA

`include/risc/isa/isa_config.hpp` is the shared ISA source of truth.

- 32-bit fixed-width instructions and 32-bit words.
- 16 general-purpose registers: `r0` through `r15`.
- `r14` is `sp`; `r15` is `ra`.
- 25 currently defined arithmetic, logical, shift, memory, control-flow, and system instructions.
- Register, immediate, and PC-relative branch formats.
- Default signed, `u` unsigned, and `h` upper-half immediate modifiers.

### Memory, CPU, and visualizer

`include/risc/isa/machine_config.hpp` defines 64 KB little-endian memory:

```text
0x0000-0x3FFF  TEXT/instruction memory
0x4000-0xBFFF  data memory
0xC000-0xFFFF  stack memory
```

Memory enforces word alignment and bounds, prevents runtime stores to TEXT, and permits the loader to write TEXT before execution. The CPU has a hardwired control unit, ALU, register file, instruction fetch, operand fetch, execute, memory access, and register writeback units. The terminal visualizer displays instruction flow, control signals, registers, flags, memory, and exceptions after each CPU step.

## Assembler

### Role and integration

The assembler translates human-written RISC201 source into the same 32-bit `isa::Word` values that the CPU fetches from instruction memory.

```text
program.asm -> Assembler -> vector<isa::Word> -> memory.load_word() -> CPU
```

The assembler runs before the CPU starts. It is not one of IF/OF/EX/MA/RW.

Source layout:

```text
include/risc/assembler/assembler.hpp  Public Assembler and AssemblerError API
src/assembler/assembler.cpp           Parser, two passes, encoders, hex writer
src/assembler_main.cpp                risc_asm command-line executable
programs/sum.asm                      Assembly version of the original demo
```

`risc_run` has two input paths:

```text
build\risc_run.exe 100
    -> uses the legacy hardcoded PROGRAM vector in src/main.cpp

build\risc_run.exe programs\sum.asm 100
    -> assembles source in memory
    -> receives vector<isa::Word>
    -> checks TEXT capacity
    -> loads words with memory.load_word()
    -> executes them in the CPU
```

`risc_asm` writes a `.hex` text file containing one eight-digit word per line. `risc_run` does not yet load `.hex` files; direct `.asm` execution assembles in memory and does not need a temporary hex file.

### Supported source syntax

- `;` starts a comment.
- Labels use a trailing colon, for example `loop:`.
- Registers are `r0` through `r15`, plus aliases `sp` and `ra`.
- Decimal and `0x`-prefixed hexadecimal immediates are accepted.
- Labels are case-insensitive and use letters/underscores first, followed by letters, digits, or underscores.
- The assembler uses the operand shapes from `isa_config.hpp`, including:

```asm
add rd, rs1, rs2_or_immediate
cmp rs1, rs2_or_immediate
mov rd, rs2_or_immediate
ld  rd, offset[rs1]
st  rs, offset[rs1]
beq label
bgt label
bc  label
bv  label
b   label
call label
ret
nop
hlt
```

### Assembly stages

1. Read a `.asm` file.
2. Preprocess comments, blank lines, and surrounding whitespace while preserving line numbers.
3. Tokenize words, commas, brackets, and colons.
4. Parse instruction shapes using shared ISA operand metadata.
5. Parse and validate registers.
6. Parse decimal/hex immediate values.
7. Pass 1: build `label -> address` mappings with a location counter.
8. Encode opcode, I bit, register, modifier, and immediate fields.
9. Encode branches with signed PC-relative offsets: `(target - current_pc) / 4`.
10. Optionally write hexadecimal output.
11. Load direct-execution output into TEXT memory beginning at `0x0000`.
12. Report file, line, and descriptive errors for malformed source or I/O failures.

Errors include missing files, unknown instructions, malformed operands, invalid registers, invalid/out-of-range immediates, invalid or duplicate labels, undefined labels, branch-range failures, output failures, and programs larger than TEXT memory.

## CMake targets

```text
risc_isa          Interface library exposing include/
risc_visualizer   Terminal visualizer static library
risc_assembler    Assembler static library
risc_run          Simulator executable; links visualizer and assembler
risc_asm          Standalone assembler executable
```

## Planned next work

1. Add directed tests for the hardwired baseline and assembler edge cases.
2. Implement a disassembler using the same ISA table and add round-trip tests.
3. Add a `.hex` file loader if standalone generated images should execute directly.
4. Complete hardwired-baseline validation and documentation.
5. Implement and compare the 4-stage and 6-stage overlapping pipeline models.
6. Add microprogrammed control, stack preprocessing/safety/visualization, interactive debugger capabilities, and evaluation tooling.

## Assignment scope

`20260823_CSC201CoA_Assignment.pdf` requires the wider RISC201 environment: two-pass assembly/disassembly, step-by-step simulation, stack support and safety, microprogrammed control, 4-stage/6-stage comparison, an optimized ALU, and evaluation. The current implementation is the hardwired five-unit baseline plus its assembler foundation.
