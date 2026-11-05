# RISC201 ALU Design & Implementation Plan
**Course:** Computer Organization & Architecture (CSC-201)  
**Primary Source:** William Stallings, *Computer Organization and Architecture* (7th Edition, Chapter 9: "Computer Arithmetic") & Prof. Smruti Sarangi  
**Target:** Optimized, Swappable 32-bit ALU Suite for RISC201 Processor Simulation Framework

---

## 1. Executive Summary & Problem Scope

Assignment Requirements 1c, 4, and 5 require:
1. **Design an ALU** with efficient arithmetic, shift, and logical operations.
2. **Implement an optimized `ALU` class** into the simulation codebase that drives execution in both the hardwired sequential CPU and the microprogrammed processors (sequential and 6-stage pipelined).
3. **Support multiple swappable algorithms** for addition, multiplication, and division to enable quantitative performance and hardware-cost evaluation (CPI, delay in FO4, operator gate counts).

This document specifies the exact algorithmic design, hardware logic, mathematical equations, flag mechanics, and software architecture based on the course lecture slides.

---

## 2. Algorithm Mapping from Course Slides & Selection

The course slides (Stallings Ch. 9) provide the exact theoretical basis for integer arithmetic. Where multiple candidate algorithms exist, the **best algorithm** is selected for primary execution, while alternative algorithms from the slides are implemented alongside it to fulfill the assignment's comparative evaluation requirement.

| Operation | Candidates in Course Slides | Selected Primary ("Best") Algorithm | Alternative Evaluator Algorithms | Time / Logic Complexity |
| :--- | :--- | :--- | :--- | :---: |
| **Addition / Subtraction** (`ADD`, `SUB`, `CMP`, Address Calc) | 1. Ripple Carry Adder (RCA)<br>2. Carry Select Adder (CSLA)<br>3. Carry Lookahead Adder (CLA) | **Carry Lookahead Adder (Tree CLA / Parallel Prefix)** *(Slides 16–26)* | • Ripple Carry Adder *(Slide 10)*<br>• Carry Select Adder ($k=4$) *(Slides 12–14)* | **$O(\log n)$** vs $O(\sqrt{n})$ vs $O(n)$ |
| **Multiplication** (`MUL`) | 1. Unsigned Shift-and-Add<br>2. Booth's Algorithm (Radix-2) | **Booth's Algorithm (Radix-2)** *(Slides 30–31)* | • Unsigned Shift-and-Add with Sign Negation *(Slides 27–29)*<br>• Modified Booth (Radix-4) | **32 cycles**, native 2's complement |
| **Division & Modulo** (`DIV`, `MOD`) | 1. Unsigned Restoring Division<br>2. Non-Restoring Division (Sarangi ref) | **Restoring Division** *(Slides 33–34)* | • Non-Restoring Division *(Sarangi Ch. 9)* | **32 cycles**, simultaneous quotient & remainder |
| **Shift** (`LSL`, `LSR`, `ASR`) | Logarithmic Multiplexer Shifter | **5-Stage Logarithmic Barrel Shifter** | Single-cycle structural simulation | **5 Mux Levels** ($16, 8, 4, 2, 1$) |
| **Logic** (`AND`, `OR`, `XOR`, `NOT`, `MOV`) | Bitwise Boolean gates | **Parallel Bitwise Logic Array** | — | **$O(1)$** |

---

## 3. Detailed Algorithmic Specifications

### 3.1 Addition & Subtraction Unit

#### A. Subtraction Hardware Principle (Slide 6)
Subtraction is performed using the adder datapath by taking the two's complement of the subtrahend:
$$A - B = A + (\sim B) + 1$$
* min_input: $A$
* sub_input: $B \oplus \text{sub\_flag}$
* carry_in ($C_0$): $\text{sub\_flag}$ ($1$ for `SUB` and `CMP`, $0$ for `ADD`)

#### B. Best Adder: Carry Lookahead Adder (CLA / Tree) (Slides 16–25)
* **Generate ($G_i$) & Propagate ($P_i$) Functions** (Slide 16–17):
  $$G_i = A_i \cdot B_i$$
  $$P_i = A_i \oplus B_i$$
  * $G_i = 1$ indicates that bit position $i$ generates a carry independently of carry-in.
  * $P_i = 1$ indicates that bit position $i$ propagates an incoming carry to bit $i+1$.

* **4-bit Lookahead Equations** (Slide 18):
  $$C_1 = G_0 + P_0 C_0$$
  $$C_2 = G_1 + P_1 G_0 + P_1 P_0 C_0$$
  $$C_3 = G_2 + P_2 G_1 + P_2 P_1 G_0 + P_2 P_1 P_0 C_0$$
  $$C_4 = G_3 + P_3 G_2 + P_3 P_2 G_1 + P_3 P_2 P_1 G_0 + P_3 P_2 P_1 P_0 C_0$$

* **Divide-and-Conquer Multi-Level Tree** (Slides 19–25):
  For 32 bits, group signals hierarchically:
  $$G_{i:j} = G_{i:k} + (P_{i:k} \cdot G_{k-1:j})$$
  $$P_{i:j} = P_{i:k} \cdot P_{k-1:j}$$
  - **Stage I (Up-tree)**: Compute group $(G, P)$ for blocks of size 2, 4, 8, 16, 32 in $\log_2(32) = 5$ levels.
  - **Stage II (Down-tree)**: Propagate intermediate carries down to each bit position in $O(\log n)$ time.
  - **Sum Output**:
    $$S_i = P_i \oplus C_i$$

#### C. Alternative 1: Ripple Carry Adder (RCA) (Slides 9–10)
* Pure sequential carry chain:
  $$S_i = A_i \oplus B_i \oplus C_i$$
  $$C_{i+1} = (A_i \cdot B_i) + (C_i \cdot (A_i \oplus B_i))$$
* Latency: $32$ gate delays ($O(n)$).

#### D. Alternative 2: Carry Select Adder (CSLA) (Slides 12–14)
* Divides 32 bits into blocks of size $k=4$ (8 blocks total).
* Block 0 uses a standard RCA with $C_0$.
* Blocks $1 \dots 7$ duplicate the adder hardware:
  - Speculative Adder 0 assumes $C_{in} = 0$.
  - Speculative Adder 1 assumes $C_{in} = 1$.
* Once carry-out from block $j-1$ arrives, a $2:1$ multiplexer selects the pre-computed sum and carry.
* Latency: $k + \frac{n}{k} \approx 4 + 8 = 12$ delays ($O(\sqrt{n})$).

---

### 3.2 Condition Codes & Flags Logic (Correct Signed Comparison)

The processor maintains 4 condition flags: $E$ (Equal), $GT$ (Greater Than), $C$ (Carry Out), and $V$ (Signed Overflow).
For `CMP A, B`, the ALU internally calculates $R = A - B$ and derives flags:

1. **Equal ($E$)**:
   $$E = (R == 0)$$
2. **Carry / Borrow ($C$)**:
   * For addition: $C = \text{CarryOut}_{31}$
   * For subtraction (`SUB`, `CMP`): $C = \text{CarryOut}_{31}$ (where CarryOut $= 1 \iff A \ge B$ unsigned; borrow $= \neg C$).
3. **Signed Overflow ($V$)**:
   Overflow occurs if the sign of the result differs from the expected mathematical sign:
   * Addition ($A + B$): $V = (A_{31} == B_{31}) \land (A_{31} \ne R_{31})$
   * Subtraction ($A - B$): $V = (A_{31} \ne B_{31}) \land (A_{31} \ne R_{31})$
   Or via adder carry-bits: $V = C_{31} \oplus C_{32}$.
4. **Signed Greater-Than ($GT$)**:
   > ⚠️ **The Critical Two's Complement Trap:**  
   > Computing $GT$ naively as `SWord(A) > SWord(B)` or `(R > 0)` is vulnerable to hardware overflow! When $A = \text{0x7FFFFFFF}$ ($+2{,}147{,}483{,}647$) and $B = \text{0x80000000}$ ($-2{,}147{,}483{,}648$), mathematically $A > B$, but $A - B$ overflows to $-1$ ($N=1, V=1$).  
   > The rigorous hardware formula is:
   $$GT = (N == V) \land \neg E$$
   where $N = R_{31}$ (sign bit of result). When overflow occurs ($V=1$), it reverses the sign bit ($N$), so $(N == V)$ cancels the inversion and guarantees mathematical correctness under all conditions.

---

### 3.3 Multiplication Unit (`MUL`)

#### Best Algorithm: Booth's Algorithm (Radix-2) (Slides 30–31)
* **Registers**:
  - $M$: Multiplicand (32-bit, sign-extended)
  - $A$: Accumulator (32-bit, initialized to 0)
  - $Q$: Multiplier (32-bit)
  - $Q_{-1}$: 1-bit flip-flop (initialized to 0)
  - $\text{Count}$: Initialized to 32
* **Step-by-step Loop (Repeated 32 times)**:
  1. Inspect bit pair $(Q_0, Q_{-1})$:
     - `00` or `11`: **No operation** (sequence of 0s or 1s).
     - `01`: $A \leftarrow A + M$ (end of string of 1s).
     - `10`: $A \leftarrow A - M$ (beginning of string of 1s).
  2. **Arithmetic Right Shift**:
     Shift $[A, Q, Q_{-1}]$ jointly by 1 bit to the right:
     - $Q_{-1} \leftarrow Q_0$
     - $Q_0 \dots Q_{30} \leftarrow Q_1 \dots Q_{31}$
     - $Q_{31} \leftarrow A_0$
     - $A_0 \dots A_{30} \leftarrow A_1 \dots A_{31}$
     - $A_{31} \leftarrow A_{31}$ (sign bit preserved!)
  3. $\text{Count} \leftarrow \text{Count} - 1$. Loop until $\text{Count} == 0$.
* **Output**:
  For RISC201 32-bit word output, the lower 32 bits reside in register $Q$.
* **Advantages over Unsigned Shift-and-Add**:
  - Handles positive and negative 2's complement numbers uniformly without pre-converting signs or post-negating.
  - Skips additions during runs of consecutive identical bits.

---

### 3.4 Division & Modulo Unit (`DIV`, `MOD`)

#### Best Algorithm: Restoring Division (Slides 33–34)
* **Registers**:
  - $M$: Divisor (32-bit magnitude)
  - $A$: Partial Remainder (initialized to 0)
  - $Q$: Dividend (32-bit magnitude)
  - $\text{Count}$: Initialized to 32
* **Step-by-step Flowchart Execution (Slide 34)**:
  1. **Shift Left**:
     Shift $[A, Q]$ jointly left by 1 bit ($A_{31}$ lost, $A_0 \leftarrow Q_{31}$, $Q_0 \leftarrow 0$).
  2. **Trial Subtraction**:
     $$A \leftarrow A - M$$
  3. **Restore Test**:
     - **If $A < 0$ ($A_{31} == 1$)**:
       $Q_0 \leftarrow 0$
       $A \leftarrow A + M$ *(Restore partial remainder)*
     - **If $A \ge 0$ ($A_{31} == 0$)**:
       $Q_0 \leftarrow 1$ *(No restore needed)*
  4. $\text{Count} \leftarrow \text{Count} - 1$. Loop until $\text{Count} == 0$.
* **Simultaneous Output**:
  - Quotient is in $Q$.
  - Remainder is in $A$.
* **Sign Correction & Corner Cases**:
  1. **Division by Zero**: If $B == 0$, immediately throw `CpuException{Exception::DivideByZero, 0}`.
  2. **Overflow Corner Case**: If $A == \text{INT\_MIN}$ and $B == -1$, quotient wraps to $\text{INT\_MIN}$ and remainder is $0$.
  3. **Signed Arithmetic Rules**:
     - $\text{Sign}(\text{Quotient}) = \text{Sign}(A) \oplus \text{Sign}(B)$
     - $\text{Sign}(\text{Remainder}) = \text{Sign}(A)$ (Remainder takes the dividend's sign, matching standard ISO C++ / Sarangi semantics).

---

### 3.5 Barrel Shifter Unit (`LSL`, `LSR`, `ASR`)

A 32-bit shift distance requires only 5 bits: $k = B[4:0]$ (range $0 \dots 31$).
The shifter is implemented as a 5-stage multiplexer cascade:
- **Stage 0**: Shift by 16 if $k[4] == 1$
- **Stage 1**: Shift by 8 if $k[3] == 1$
- **Stage 2**: Shift by 4 if $k[2] == 1$
- **Stage 3**: Shift by 2 if $k[1] == 1$
- **Stage 4**: Shift by 1 if $k[0] == 1$

Modes:
- `LSL`: Shift left, fill with `0`.
- `LSR`: Shift right, fill with `0`.
- `ASR`: Shift right, fill with copy of sign bit ($A_{31}$).

---

## 4. Hardware Complexity & Delay Model Comparison

> **Replaced (Nov 2026):** the figures below were estimates typed into the code. The units are now built from gates as they run, and their delay, area and µcycles are counted. See `docs/alu_timing.md` for the counted figures and how they change the processors' timing.

To provide quantitative data for the project report (Item 5), each algorithm exposes structural metrics:

| Sub-unit | Algorithm | Logic Depth ($D$) | Operator Nodes ($N$) | FO4 Equivalent Delay |
| :--- | :--- | :---: | :---: | :---: |
| **Adder** | Ripple-Carry Adder | $n = 32$ | $32$ FAs | $\sim 64\text{ FO4}$ |
| | Carry-Select Adder ($k=4$) | $k + n/k = 12$ | $56$ FAs + $32$ Muxes | $\sim 24\text{ FO4}$ |
| | Carry-Lookahead / Prefix (Tree) | $\log_2(n) = 5$ | $\sim 80\text{--}129$ prefix nodes | $\sim 8\text{ FO4}$ |
| **Multiplier** | Shift-and-Add | $32$ steps | 1 Adder + 64-bit shifter | $32 \times T_{\text{add}}$ |
| | Booth Radix-2 | $32$ steps | 1 Adder/Sub + 65-bit shifter | Variable ($16\text{--}32 \times T_{\text{add}}$) |
| **Divider** | Restoring Division | $32$ steps | 1 Subtractor + 1 Restorer | $32 \times (T_{\text{sub}} + T_{\text{restore}})$ |
| **Shifter** | 5-stage Barrel Shifter | $5$ levels | $5 \times 32 = 160$ 2:1 Muxes | $\sim 5\text{ FO4}$ |

---

## 5. Software Architecture & File Layout

We will create a clean, dedicated modular directory `include/risc/alu/`:

```
include/risc/alu/
├── alu_types.hpp        # Algorithm enums (AdderAlgorithm, MultiplierAlgorithm, DividerAlgorithm) & metrics struct
├── adders.hpp           # Class Adders: ripple_carry(), carry_select(), carry_lookahead()
├── multipliers.hpp      # Class Multipliers: shift_add(), booth_radix2(), booth_radix4()
├── dividers.hpp         # Class Dividers: restoring(), non_restoring()
├── shifter.hpp          # Class Shifter: barrel_shift(a, amount, mode)
└── alu.hpp              # Class OptimizedALU: unifies execution, algorithm selection, and flags
```

### Integration Points
1. **`include/risc/micro/datapath/alu.hpp`**:
   The microprogrammed processor's ALU will instantiate `OptimizedALU` and forward commands from `AluOp` (`Add`, `Sub`, `Cmp`, `Mul`, `Div`, `Mod`, `Lsl`, etc.) directly to the optimized algorithms.
2. **`include/risc/cpu/alu.hpp`**:
   The hardwired sequential CPU will route operations through `OptimizedALU`.
3. **Unit & Benchmark Test Executable (`src/alu_test_main.cpp`)**:
   A dedicated test runner that:
   - Exhaustively validates arithmetic correctness against golden values.
   - Tests boundary edge cases: signed overflow, maximum positives, negative values, division by zero, `INT_MIN / -1`.
   - Compares the cycle and delay metrics across RCA, Carry-Select, CLA, Shift-Add, and Booth.

---

## 6. Implementation Checklist & Phasing

- [ ] **Phase 1: Shared ALU Headers (`include/risc/alu/`)**
  - [ ] Implement `adders.hpp`: Ripple-Carry, Carry-Select ($k=4$), and Tree Carry-Lookahead.
  - [ ] Implement flag calculation with signed $(N == V) \land \neg E$ logic.
  - [ ] Implement `multipliers.hpp`: Booth's Radix-2 algorithm and Shift-and-Add.
  - [ ] Implement `dividers.hpp`: Restoring division algorithm with simultaneous quotient/remainder and divide-by-zero detection.
  - [ ] Implement `shifter.hpp`: 5-level logarithmic barrel shifter.
  - [ ] Implement `alu.hpp`: Unified class with runtime algorithm switching.
- [ ] **Phase 2: Unit Testing Suite**
  - [ ] Add `test_alu` target in `CMakeLists.txt`.
  - [ ] Verify signed multiplication (`-5 * 6`, `-10 * -20`, `0x7FFFFFFF * 2`).
  - [ ] Verify division remainders (`-14 % 3 == -2`, `14 % -3 == 2`).
  - [ ] Verify signed comparison flags with overflow (`0x7FFFFFFF` vs `0x80000000`).
- [ ] **Phase 3: Core Integration**
  - [ ] Connect `risc::micro::ALU` in `include/risc/micro/datapath/alu.hpp`.
  - [ ] Connect `risc::cpu::ALU` in `include/risc/cpu/alu.hpp`.
  - [ ] Run `risc_6stage_run.exe programs/factorial.asm` to confirm 100% equivalence.
- [ ] **Phase 4: Evaluation Metrics & Reporting**
  - [ ] Emit algorithm characteristics table (depth, node count, delay in FO4).
