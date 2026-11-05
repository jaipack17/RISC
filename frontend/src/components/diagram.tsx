import { useEffect, useRef, useState } from "react";
import type { CpuState, Packet, StageState } from "../types";
import { hex, stageNames, stagePacket } from "../types";

// Drawing pieces shared by the six-stage and the four-stage block diagrams: the vertical
// layout, wires, registers on a bus, units, and the instruction tokens of the black-box view.

export const CTRL_Y = 14; // beat controller
export const CTRL_H = 40;
export const UC_Y = 92; // µcontrol units
export const UC_H = 62;
export const BRANCH_Y = 178; // branch feedback wire
export const STAGE_Y = 196; // stage frames
export const STAGE_H = 300;
export const BUS_Y = STAGE_Y + 92;
export const BUS_H = 16;
export const SLOT_Y = BUS_Y + BUS_H + 34;
export const SLOT_H = 32;
export const UNIT_Y = SLOT_Y + SLOT_H + 26;
export const MEM_Y = STAGE_Y + STAGE_H + 46; // memories and register file
export const MEM_H = 92;
export const WB_Y = MEM_Y + MEM_H + 26; // write-back wire
export const H = WB_Y + 18;
export const TOKEN_Y = STAGE_Y + 120;
export const TOKEN_H = 40;

export function short(value: number): string {
  const v = value >>> 0;
  return v <= 0xffff ? hex(v, 4) : hex(v, 8);
}

// Where the registers of a stage sit on its bus
export function slotGeometry(stageX: number, stageW: number, count: number) {
  const w = Math.min(64, (stageW - 20 - (count - 1) * 6) / count);
  const total = count * w + (count - 1) * 6;
  return { x0: stageX + (stageW - total) / 2, w };
}

export function Markers() {
  return (
    <defs>
      {["data", "feedback", "control"].map((k) => (
        <marker key={k} id={"head-" + k} viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto">
          <path d="M0 0 L10 5 L0 10 z" className={"head head-" + k} />
        </marker>
      ))}
    </defs>
  );
}

export function Wire({ d, on, kind = "data", flow = true }: { d: string; on: boolean; kind?: string; flow?: boolean }) {
  return <path d={d} className={`w w-${kind}${on ? " on" : ""}${on && flow ? " flow" : ""}`} />;
}

export interface SlotSpec {
  name: string; // the microcode register name, e.g. "regSrc"
  label?: string;
  value: string;
}

// A register on a stage's local bus
export function Slot({ x, w, spec, read, written }: { x: number; w: number; spec: SlotSpec; read: boolean; written: boolean }) {
  return (
    <g className={"slot" + (read ? " read" : "") + (written ? " written" : "")}>
      <rect x={x} y={SLOT_Y} width={w} height={SLOT_H} />
      <text x={x + w / 2} y={SLOT_Y + 13} textAnchor="middle" className="slot-name">
        {spec.label ?? spec.name}
      </text>
      <text x={x + w / 2} y={SLOT_Y + 26} textAnchor="middle" className="slot-value">
        {spec.value}
      </text>
    </g>
  );
}

export function Unit({
  x,
  y,
  w,
  h,
  title,
  lines,
  on,
  kind = "unit",
}: {
  x: number;
  y: number;
  w: number;
  h: number;
  title: string;
  lines?: string[];
  on: boolean;
  kind?: string;
}) {
  return (
    <g className={`${kind}${on ? " on" : ""}`}>
      <rect x={x} y={y} width={w} height={h} />
      <text x={x + w / 2} y={y + 16} textAnchor="middle" className="unit-title">
        {title}
      </text>
      {(lines ?? []).map((line, i) => (
        <text key={i} x={x + w / 2} y={y + 31 + i * 13} textAnchor="middle" className="unit-line">
          {line}
        </text>
      ))}
    </g>
  );
}

// The text on a stage's local bus for the microinstruction it just ran
export function busText(st: StageState): string {
  if (!st.ran) {
    return "";
  }
  const s = st.signals;
  const parts: string[] = [];
  if (s.loadIR) {
    parts.push("ir ← mem[pc]");
  }
  if (s.decode) {
    parts.push("decode ir");
  }
  if (s.transfer) {
    let from = s.src;
    if (s.mux === "immediate") {
      from = String(s.imm);
    } else if (s.mux === "microAdder") {
      from = `${s.src}+${s.imm}`;
    }
    if (s.dest === "") {
      parts.push(`${from} = ${s.imm} ? ${st.isMBranch ? "jump" : "no"}`);
    } else {
      parts.push(`${s.dest} ← ${from} (${short(st.readBus)})`);
    }
  }
  if (s.args !== "") {
    parts.push(`<${s.args}>`);
  }
  return parts.join("   ");
}

export function instructionText(state: CpuState, i: number, p: Packet): string {
  if (i === 0 && !state.beat.inProgress) {
    return state.status === "running" ? `next: fetch ${hex(state.units.fetch.pc)}` : "";
  }
  if (p.bubble) {
    return "bubble";
  }
  if (p.hasException) {
    return `${hex(p.currentPc)}  ${p.exception}`;
  }
  return `#${p.id}  ${p.assembly}`;
}

export function tokenText(p: Packet): string {
  if (p.hasException) {
    return `${hex(p.currentPc)} ${p.exception}`;
  }
  return p.assembly;
}

// An instruction that just left the pipeline: it finished the last stage, or a taken branch flushed it
export interface Ghost {
  id: number;
  stage: number;
  text: string;
  kind: "retired" | "flushed";
}

// Instruction id -> the stage it is in
function positions(state: CpuState): Map<number, number> {
  const map = new Map<number, number>();
  for (let i = 0; i < stageNames(state.processor).length; i++) {
    const p = stagePacket(state, i);
    if (!p.bubble && p.id > 0) {
      map.set(p.id, i);
    }
  }
  return map;
}

// The instructions that left since the last update, to animate them out
export function useGhosts(state: CpuState): Ghost[] {
  const previous = useRef<{
    state: CpuState;
    where: Map<number, number>;
  } | null>(null);
  const [ghosts, setGhosts] = useState<Ghost[]>([]);

  useEffect(() => {
    const where = positions(state);
    const before = previous.current;
    previous.current = { state, where };
    if (!before || before.state.processor !== state.processor || state.counters.microcycles < before.state.counters.microcycles) {
      setGhosts([]); // reset, a new program or the other processor
      return;
    }
    const last = stageNames(state.processor).length - 1;
    const gone: Ghost[] = [];
    before.where.forEach((stageIndex, id) => {
      if (!where.has(id)) {
        const p = stagePacket(before.state, stageIndex);
        const kind = stageIndex === last ? "retired" : "flushed";
        gone.push({ id, stage: stageIndex, text: tokenText(p), kind });
      }
    });
    if (gone.length > 0) {
      setGhosts(gone);
      const timer = setTimeout(() => setGhosts([]), 900);
      return () => clearTimeout(timer);
    }
  }, [state]);

  return ghosts;
}
