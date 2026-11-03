// The JSON sent by the C++ backend (backend/session.cpp). Keep the two in step.

export const STAGE_NAMES = ["IF", "OF1", "OF2", "EX", "MA", "RW"] as const;
export type StageName = (typeof STAGE_NAMES)[number];

// ---- /api/program: changes only when a program is loaded ----

export interface ListingLine {
  line: number;
  text: string;
  hasInstruction: boolean;
  address: number;
  word: number;
  assembly: string;
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
}

export interface Latches {
  fetched: Packet;
  ifOf1: Packet;
  of1Of2: Packet;
  of2Ex: Packet;
  exMa: Packet;
  maRw: Packet;
}

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
  status: "running" | "halted" | "exception";
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
  beatLog: BeatRecord[];
}

// The packet each stage works on (IF builds a new one in "fetched")
export function stagePacket(state: CpuState, stage: number): Packet {
  const order: (keyof Latches)[] = ["fetched", "ifOf1", "of1Of2", "of2Ex", "exMa", "maRw"];
  return state.latches[order[stage]];
}

export function hex(value: number, digits = 4): string {
  return "0x" + (value >>> 0).toString(16).toUpperCase().padStart(digits, "0");
}
