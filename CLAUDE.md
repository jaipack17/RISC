# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project status

This repository is currently empty except for the assignment brief
(`20260823_CSC201CoA_Assignment.pdf`). No source code, build system, or
language choice exists yet. There are no build/lint/test commands to
document until the implementation is started — do not invent any.

When implementation begins, update this file with the actual build/run/test
commands and the real module layout once they exist.

## Problem formulation (from the assignment brief)

Course: Computer Organization & Architecture (CSC-201), Autumn 2026 — "Processor
Design" assignment. The deliverable is a custom RISC ISA called **RISC201**
together with a full simulation/tooling suite around it.

### 1. Processor design
- A **4-stage** and a **6-stage** RISC201 ISA/pipeline implementation, with a
  comparison of their behavior.
- A **microprogrammed control unit** for the processor (horizontal or
  vertical microcode model — implementer's choice; the brief asks for extra
  credit for doing both and comparing them).
- A dedicated **ALU** supporting efficient arithmetic and logical operations.

### 2. Software environment / simulation framework
- A **two-pass assembler**: RISC201 assembly → binary or hex.
- A **disassembler**: binary/hex word strings → reconstructed assembly.
- An **interactive CLI** to inspect the register file and instruction
  pipeline.
- A **step-by-step CPU execution simulator with debugger** (interactive CLI
  debugger).
- A **stack preprocessor** implementing macro expansion for full-descending
  stack operations.
- **Stack boundary safety verification** plus a **runtime exception
  framework** for memory protection against stack corruption.
- An **ASCII stack memory frame visualizer** (active stack memory
  visualizer).

### 3. Microprogrammed control integration
- Wire the microprogrammed control logic directly into the CPU simulator so
  RISC201 executes via **control-memory microcode** rather than hardwired
  logic.
- Support and compare **horizontal vs. vertical** microprogrammed control.
- The CLI simulator/debugger must expose the **microPC's step-by-step
  state** and raise exceptions where appropriate.

### 4. ALU integration
- An optimized `RISC201 ALU` class/module driving all arithmetic, shift, and
  logical operations used by the simulator.

### 5. Evaluation
- Evaluate processor, control unit, and ALU across: 4-stage vs. 6-stage
  ISA, multiple assembly test programs, horizontal vs. vertical
  microprogrammed control (optional), and different ALU algorithms.

### Architectural implications for implementation

Because the deliverables above are explicitly coupled (the control unit must
be swappable between horizontal/vertical microcode, and the pipeline must be
swappable between 4-stage/6-stage while sharing the same ISA, assembler,
disassembler, and ALU), the codebase should be structured from the start so
that:
- The **ISA definition** (instruction encoding, opcodes, register set) is a
  single shared source of truth used by the assembler, disassembler, and
  both pipeline variants — avoid duplicating instruction semantics per
  pipeline depth.
- The **ALU** is a standalone component independent of pipeline stage count,
  reusable by both the 4-stage and 6-stage datapaths.
- The **control unit** is pluggable: the CPU simulator should be able to
  select horizontal-microcode, vertical-microcode (and if applicable,
  hardwired) control without changing the ALU/ISA/pipeline-stage logic.
- The **stack safety/exception framework** is a cross-cutting concern
  (assembler-time macro expansion + runtime bounds checking) rather than
  something bolted onto one specific pipeline variant.
- The CLI simulator/debugger is the integration point that ties
  register-file state, pipeline stage state, microPC state, and the stack
  visualizer together — expect this to be the most stateful, highest-fan-in
  module.
