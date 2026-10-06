# Running programs on the microprogrammed processor (`risc_micro_run`)

`src/micro_main.cpp` builds the `risc_micro_run` tool. It runs a RISC201 assembly
program on the **microprogrammed** processor (vertical microcode). It is the
microprogrammed counterpart of `risc_run`, which runs programs on the hardwired
processor.

## Usage

```sh
cmake -S . -B build && cmake --build build
./build/risc_micro_run <program.asm> [microcode.masm]
```

- `program.asm`: the program to run (RISC201 assembly).
- `microcode.masm`: the microcode to use. Optional, the default is
  `microcode/vertical.masm`. Its syntax is described in `docs/microassembly.md`.

Example:

```sh
./build/risc_micro_run programs/sum.asm
```

## What it does

### 1. Loads the program into memory

The program is assembled with the two-pass assembler (`risc::assembler::Assembler`)
into 32-bit machine code, and each word is written into the instruction region of
memory (starting at `0x0000`) with `Memory::write_instruction_word`. This is the same
machine code the hardwired processor runs.

### 2. Loads the microcode into the µcontrol unit

The microcode file is assembled with the microassembler
(`risc::microassembler::Microassembler`) into:

- 45-bit microinstruction words, written into the microprogram memory with
  `MicroControlUnit::write_microprogram_words`, and
- the switch table (one entry per instruction that has microcode), written with
  `SwitchUnit::set_start`.

If the program or the microcode has an error, the tool prints it (with the line
number) and exits.

### 3. Runs one microinstruction per cycle

Each call to `MicroControlUnit::step` is one cycle: the µcontrol unit reads the
microinstruction at the µpc, decodes it, applies it to the datapath, and chooses the
next µpc. After every cycle the tool prints one line:

```
cycle 1     µpc=0    mloadIR  -> µpc=1    pc=0x0000
cycle 2     µpc=1    mdecode  -> µpc=2    pc=0x0000
cycle 3     µpc=2    madd     -> µpc=3    pc=0x0004
cycle 4     µpc=3    mswitch  -> µpc=4    pc=0x0004
```

- `µpc`: the microinstruction executed this cycle, and the µpc chosen for the next one.
- the microinstruction type (`mloadIR`, `mmov`, ...).
- `pc`: the program counter after this cycle.

### 4. Stops

The run ends in one of three ways:

| Reason | Output |
|---|---|
| The `hlt` routine sends `<halt>` | `Halted after N cycles, pc=...`, followed by all 16 registers |
| An exception (`IllegalInstruction`, `DivideByZero`, `WriteToText`, ...) | `Exception: <name> in the instruction at pc=... (cycle N)` |
| 100000 cycles without halting | `Stopped (cycle limit) ...`, followed by all 16 registers |

For exceptions, the reported `pc` is the address of the instruction that was running.
The preamble moves `pc` forward before an instruction can fail, so the tool
remembers `pc` each time the µpc returns to 0 (the start of the preamble).

An instruction with **no microcode** in the `.masm` file stops the run with
`IllegalInstruction` (the switch unit has no entry for its opcode).

## Current state

`microcode/vertical.masm` contains only the preamble and the `hlt` routine. So:

- a program containing only `hlt` halts after 5 cycles;
- any other instruction stops with `IllegalInstruction` until its routine is added
  to `microcode/vertical.masm`.

## Files involved

| File | Role |
|---|---|
| `src/micro_main.cpp` | this tool |
| `include/risc/assembler/assembler.hpp` | assembles the program |
| `include/risc/microassembler/microassembler.hpp` | assembles the microcode |
| `microcode/vertical.masm` | the microcode |
| `include/risc/micro/control/micro_control_unit.hpp` | µpc, microprogram memory, execute unit, µfetch multiplexer |
| `include/risc/micro/datapath/datapath.hpp` | the units and the shared bus |
| `include/risc/memory/memory.hpp` | memory |
