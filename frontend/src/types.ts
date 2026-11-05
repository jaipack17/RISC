// The JSON sent by the C++ backend (backend/session.cpp). Keep the two in step.

export type Processor = "six" | "four";

export const STAGE_NAMES = ["IF", "OF1", "OF2", "EX", "MA", "RW"] as const;
export const FOUR_STAGE_NAMES = ["IF", "OF", "EX", "MW"] as const;
export type StageName = (typeof STAGE_NAMES)[number] | (typeof FOUR_STAGE_NAMES)[number];

// The stage names of a processor, IF first
export function stageNames(processor: Processor): readonly StageName[] {
  return processor === "four" ? FOUR_STAGE_NAMES : STAGE_NAMES;
}

// The packet each stage works on, by key in CpuState.latches (IF builds a new one in "fetched")
const STAGE_LATCHES: Record<Processor, string[]> = {
  six: ["fetched", "ifOf1", "of1Of2", "of2Ex", "exMa", "maRw"],
  four: ["fetched", "ifOf", "ofEx", "exMw"],
};

// The pipeline latches between the stages, in order, with their names
export function latchList(state: CpuState): { name: string; packet: Packet }[] {
  const names = stageNames(state.processor);
  const keys = STAGE_LATCHES[state.processor];
  const list: { name: string; packet: Packet }[] = [];
  for (let i = 1; i < keys.length; i++) {
    list.push({ name: `${names[i - 1]}/${names[i]}`, packet: state.latches[keys[i]] });
  }
  return list;
}

// ---- /api/program: changes only when a program is loaded ----

export interface ListingLine {
  line: number;
  text: string;
  hasInstruction: boolean;
  address: number;
  word: number;
  assembly: string;
  macro: string; // the macro this instruction came from, e.g. "push r1" ("" if none)
}

export interface StageWord {
  address: number;
  word: number;
  sourceLine: number; // 0 for an end marker
  part: boolean; // only part of the source line runs in this stage
  text: string;
  type: string;
  src: string;
  dest: string;
  imm: number;
  branchTarget: number;
  args: string;
  argsUnit: string;
}

export interface SwitchEntry {
  opcode: number;
  address: number;
  mnemonic: string;
}

export interface StageProgram {
  name: StageName;
  words: StageWord[];
  entries: SwitchEntry[];
}

export interface ProgramInfo {
  processor: Processor;
  name: string;
  source: string;
  listing: ListingLine[];
  microcode: { path: string; lines: string[] };
  stages: StageProgram[];
}

// ---- /api/state: changes every microcycle ----

export interface Packet {
  bubble: boolean;
  id: number;
  currentPc: number;
  nextPc: number;
  ir: number;
  assembly: string;
  I: boolean;
  rd: number;
  rs1: number;
  rs2: number;
  immx: number;
  branchTarget: number;
  A: number;
  B: number;
  regVal: number;
  regData: number;
  aluResult: number;
  ldResult: number;
  hasException: boolean;
  exception: string;
}

export interface Signals {
  loadIR: boolean;
  decode: boolean;
  transfer: boolean;
  src: string;
  dest: string;
  mux: string;
  imm: number;
  args: string;
  argsUnit: string;
  next: string;
  branchTarget: number;
}

export interface StageState {
  name: StageName;
  upc: number;
  done: boolean;
  halted: boolean;
  ran: boolean; // ran a microinstruction in the last microcycle
  lastUpc: number;
  sourceLine: number;
  part: boolean;
  signals: Signals;
  writeBus: number;
  readBus: number;
  isMBranch: boolean;
  aluWait: number; // µcycles the ALU still needs for the command at lastUpc
}

// six-stage: fetched, ifOf1, of1Of2, of2Ex, exMa, maRw; four-stage: fetched, ifOf, ofEx, exMw
export type Latches = Record<string, Packet>;

export interface BeatCell {
  empty: boolean;
  id: number;
  pc: number;
  assembly: string;
  waited: boolean;
  exception: boolean;
}

export interface BeatRecord {
  beat: number;
  microcycles: number;
  taken: boolean;
  target: number;
  cells: BeatCell[]; // one per stage, IF first
}

export interface CpuState {
  processor: Processor;
  status: "running" | "halted" | "exception";
  alu: AluConfig;
  exception: { kind: string; pc: number } | null;
  counters: {
    microcycles: number;
    beats: number;
    instructions: number;
    stallBeats: number;
    takenBranches: number;
  };
  beat: {
    inProgress: boolean;
    number: number;
    microcycle: number;
    stall: boolean;
    stallStage: StageName;
    fetching: boolean;
    branchTaken: boolean;
    branchPc: number;
  };
  stages: StageState[];
  latches: Latches;
  units: {
    fetch: { pc: number; ir: number };
    decode: { I: boolean; rd: number; rs1: number; rs2: number; immx: number; branchTarget: number };
    registerFile: {
      readSrc1: number;
      regVal1: number;
      readSrc2: number;
      regVal2: number;
      writeSrc: number;
      regData: number;
    };
    alu: { A: number; B: number; aluResult: number; flagE: boolean; flagGT: boolean };
    memoryUnit: { mar: number; mdr: number; ldResult: number };
  };
  registers: number[];
  memory: { address: number; value: number }[];
  stack: StackState;
  beatLog: BeatRecord[];
}

// The stack region around sp (full-descending: sp == end is an empty stack)
export interface StackState {
  sp: number;
  base: number;
  end: number;
  words: { address: number; value: number; ra: boolean }[]; // ascending addresses, from just below sp to the top; ra: saved from r15
}

// ---- /api/benchmarks, /api/compare, /api/benchmark: fresh runs of all three processors ----

export interface BenchmarkInfo {
  name: string; // e.g. "benchmarks/alg01_bubble_sort.asm"
  title: string;
  about: string;
  category: string;
  expect: string;
}

export interface RunStatus {
  status: "halted" | "exception" | "limit";
  exception: { kind: string; pc: number } | null;
  microcycles: number;
  instructions: number;
}

export interface StageMeasure {
  name: string;
  busy: number; // microcycles the stage ran a microinstruction
  critical: number; // beats in which it had the longest section
  waits: number; // beats in which it waited for a data hazard
  controlStoreWords: number;
}

export interface TimelineRow {
  id: number;
  pc: number;
  assembly: string;
  cells: [number, number, boolean][]; // beat, stage, waited
}

export interface PipelineRun extends RunStatus {
  beats: number;
  fetched: number;
  stallBeats: number;
  takenBranches: number;
  stages: StageMeasure[];
  beatLengths: { length: number; count: number }[];
  // each beat charged to what left the last stage: an instruction (work) or a bubble's reason
  cycleStack: Record<"work" | "stall" | "flush" | "fillDrain", { microcycles: number; beats: number }>;
  timeline?: {
    beats: { beat: number; microcycles: number; stall: number; taken: boolean }[];
    rows: TimelineRow[];
  };
}

export interface CompareResult {
  program: BenchmarkInfo;
  four: PipelineRun;
  six: PipelineRun;
  sequential: RunStatus & { controlStoreWords: number };
  agree: boolean;
  differences: string;
  registers: number[];
}

// The packet a stage works on
export function stagePacket(state: CpuState, stage: number): Packet {
  return state.latches[STAGE_LATCHES[state.processor][stage]];
}

export function hex(value: number, digits = 4): string {
  return "0x" + (value >>> 0).toString(16).toUpperCase().padStart(digits, "0");
}

// The ALU's algorithms (backend: alu/alu_options.hpp)
export type AdderKey = "ripple" | "select" | "cla" | "prefix";
export type MultiplierKey = "shiftadd" | "booth2" | "booth4";
export type DividerKey = "restoring" | "nonrestoring";

export interface AluConfig {
  adder: AdderKey;
  multiplier: MultiplierKey;
  divider: DividerKey;
}

export const ADDER_NAMES: Record<AdderKey, string> = {
  ripple: "Ripple-carry",
  select: "Carry-select",
  cla: "Carry-lookahead",
  prefix: "Kogge-Stone",
};

export const MULTIPLIER_NAMES: Record<MultiplierKey, string> = {
  shiftadd: "Shift-and-add",
  booth2: "Booth radix-2",
  booth4: "Booth radix-4",
};

export const DIVIDER_NAMES: Record<DividerKey, string> = {
  restoring: "Restoring",
  nonrestoring: "Non-restoring",
};

// GET /api/alu: every unit measured from its gates, and the benchmarks run with each algorithm
export interface AdderMeasure {
  key: AdderKey;
  name: string;
  delay: number; // gate delays
  area: number; // unit-gate area
  compareDelay: number;
  addMicrocycles: number;
  compareMicrocycles: number;
}

export interface IterativeMeasure {
  stepDelay: number;
  area: number;
  min: number; // µcycles per operation
  average: number;
  max: number;
}

export interface AluRunTotals {
  microcycles: number;
  instructions: number;
  perProgram: number[];
}

export interface AluRun {
  unit: "default" | "adder" | "multiplier" | "divider";
  alu: AluConfig;
  four: AluRunTotals;
  six: AluRunTotals;
  sequential: AluRunTotals;
  agree: number;
}

export interface AluReport {
  units: {
    microcycleLength: number;
    fastestAdder: AdderKey;
    defaults: AluConfig;
    adders: AdderMeasure[];
    multipliers: { key: MultiplierKey; name: string; byAdder: Record<AdderKey, IterativeMeasure> }[];
    dividers: { key: DividerKey; name: string; byAdder: Record<AdderKey, IterativeMeasure> }[];
  };
  programs: BenchmarkInfo[];
  runs: AluRun[];
}
