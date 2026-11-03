import { useEffect, useRef, useState } from "react";
import type { CpuState, Packet, ProgramInfo, StageState } from "../types";
import { STAGE_NAMES, hex, stagePacket } from "../types";

// Block diagram of the six-stage pipelined microprogrammed processor, drawn in the style of
// Sarangi's figures 9.22 (microprogrammed data path) and 10.17 (pipeline with interlocks):
// per-stage µcontrol units, per-stage local buses with their registers, pipeline latches as
// vertical bars, memories and the register file below, the branch and write-back paths as
// thick feedback wires, and the beat controller with its stall / bubble / flush lines on top.
// In the black-box view the stages are solid blocks that hide their buses and registers;
// each instruction is a token that slides to the next stage at the end of a beat.

const SW = 214; // stage width
const GAP = 44; // space between stages, the latch sits in the middle
const LEFT = 24;
const W = LEFT * 2 + 6 * SW + 5 * GAP;

const CTRL_Y = 14; // beat controller
const CTRL_H = 40;
const UC_Y = 92; // µcontrol units
const UC_H = 62;
const BRANCH_Y = 178; // branch feedback wire
const STAGE_Y = 196; // stage frames
const STAGE_H = 300;
const BUS_Y = STAGE_Y + 92;
const BUS_H = 16;
const SLOT_Y = BUS_Y + BUS_H + 34;
const SLOT_H = 32;
const UNIT_Y = SLOT_Y + SLOT_H + 26;
const MEM_Y = STAGE_Y + STAGE_H + 46; // memories and register file
const MEM_H = 92;
const WB_Y = MEM_Y + MEM_H + 26; // write-back wire
const H = WB_Y + 18;

function sx(i: number) {
  return LEFT + i * (SW + GAP);
}

function latchX(i: number) {
  return sx(i) + SW + GAP / 2 - 9;
}

function short(value: number): string {
  const v = value >>> 0;
  return v <= 0xffff ? hex(v, 4) : hex(v, 8);
}

// Where the registers of a stage sit on its bus
function slotGeometry(i: number, count: number) {
  const w = Math.min(64, (SW - 20 - (count - 1) * 6) / count);
  const total = count * w + (count - 1) * 6;
  return { x0: sx(i) + (SW - total) / 2, w };
}

// ---- small drawing pieces ----

function Wire({ d, on, kind = "data", flow = true }: { d: string; on: boolean; kind?: string; flow?: boolean }) {
  return <path d={d} className={`w w-${kind}${on ? " on" : ""}${on && flow ? " flow" : ""}`} />;
}

interface SlotSpec {
  name: string; // the microcode register name, e.g. "regSrc"
  label?: string;
  value: string;
}

// A register on a stage's local bus, like the small boxes on the shared bus in Fig 9.22
function Slot({ x, w, spec, read, written }: { x: number; w: number; spec: SlotSpec; read: boolean; written: boolean }) {
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

function Unit({ x, y, w, h, title, lines, on, kind = "unit" }: {
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

// ---- what each stage shows ----

function slotsFor(state: CpuState, i: number, p: Packet): SlotSpec[] {
  const u = state.units;
  const rf = u.registerFile;
  switch (i) {
    case 0:
      return [
        { name: "pc", value: short(u.fetch.pc) },
        { name: "ir", value: hex(u.fetch.ir, 8) },
      ];
    case 1:
      return [
        { name: "A", value: short(p.A) },
        { name: "regSrc", value: "r" + rf.readSrc1 },
        { name: "regVal", value: short(rf.regVal1) },
      ];
    case 2:
      return [
        { name: "regSrc", value: "r" + rf.readSrc2 },
        { name: "regVal", value: short(rf.regVal2) },
        { name: "B", value: short(p.B) },
        { name: "regData", label: "regData", value: short(p.regData) },
      ];
    case 3:
      return [
        { name: "A", value: short(u.alu.A) },
        { name: "B", value: short(u.alu.B) },
        { name: "aluResult", label: "aluRes", value: short(u.alu.aluResult) },
      ];
    case 4:
      return [
        { name: "mar", value: short(u.memoryUnit.mar) },
        { name: "mdr", value: short(u.memoryUnit.mdr) },
        { name: "ldResult", label: "ldRes", value: short(u.memoryUnit.ldResult) },
      ];
    default:
      return [
        { name: "regSrc", value: "r" + rf.writeSrc },
        { name: "regData", value: short(rf.regData) },
      ];
  }
}

// The text on a stage's local bus for the microinstruction it just ran
function busText(st: StageState): string {
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

function instructionText(state: CpuState, i: number, p: Packet): string {
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

const TOKEN_Y = STAGE_Y + 120;
const TOKEN_H = 40;

// An instruction that just left the pipeline: it finished RW, or a taken branch flushed it
interface Ghost {
  id: number;
  stage: number;
  text: string;
  kind: "retired" | "flushed";
}

// Instruction id -> the stage it is in
function positions(state: CpuState): Map<number, number> {
  const map = new Map<number, number>();
  for (let i = 0; i < STAGE_NAMES.length; i++) {
    const p = stagePacket(state, i);
    if (!p.bubble && p.id > 0) {
      map.set(p.id, i);
    }
  }
  return map;
}

function tokenText(p: Packet): string {
  if (p.hasException) {
    return `${hex(p.currentPc)} ${p.exception}`;
  }
  return p.assembly;
}

export default function Datapath({ state, program, blackBox }: { state: CpuState; program: ProgramInfo; blackBox: boolean }) {
  const b = state.beat;
  const stallAt = b.inProgress && b.stall ? STAGE_NAMES.indexOf(b.stallStage) : -1;
  const stage = (i: number) => state.stages[i];
  const ran = (i: number) => stage(i).ran;
  const argsUnit = (i: number) => (ran(i) ? stage(i).signals.argsUnit : "");
  const args = (i: number) => (ran(i) ? stage(i).signals.args : "");

  const previous = useRef<{ state: CpuState; where: Map<number, number> } | null>(null);
  const [ghosts, setGhosts] = useState<Ghost[]>([]);

  // Find the instructions that left since the last update, to animate them out
  useEffect(() => {
    const where = positions(state);
    const before = previous.current;
    previous.current = { state, where };
    if (!before || state.counters.microcycles < before.state.counters.microcycles) {
      setGhosts([]); // reset or a new program
      return;
    }
    const gone: Ghost[] = [];
    before.where.forEach((stageIndex, id) => {
      if (!where.has(id)) {
        const p = stagePacket(before.state, stageIndex);
        const kind = stageIndex === STAGE_NAMES.length - 1 ? "retired" : "flushed";
        gone.push({ id, stage: stageIndex, text: tokenText(p), kind });
      }
    });
    if (gone.length > 0) {
      setGhosts(gone);
      const timer = setTimeout(() => setGhosts([]), 900);
      return () => clearTimeout(timer);
    }
  }, [state]);

  // Wires to the memories and the register file start at the registers, or at the bottom of a black box
  const portY = blackBox ? STAGE_Y + STAGE_H : SLOT_Y + SLOT_H;

  const memFetch = ran(0) && stage(0).signals.loadIR;
  const memData = argsUnit(4) === "memory";
  const read1 = args(1) === "read";
  const read2 = args(2) === "read";
  const write = args(5) === "write";
  const aluOn = argsUnit(3) === "alu";
  const branch = b.branchTaken;

  // x of the middle of register `name` in stage i (for the wires to memories and the register file)
  const slotX = (i: number, name: string) => {
    const slots = slotsFor(state, i, stagePacket(state, i));
    const g = slotGeometry(i, slots.length);
    const k = slots.findIndex((spec) => spec.name === name);
    return g.x0 + k * (g.w + 6) + g.w / 2;
  };

  const latches = [state.latches.ifOf1, state.latches.of1Of2, state.latches.of2Ex, state.latches.exMa, state.latches.maRw];
  const latchNames = ["IF/OF1", "OF1/OF2", "OF2/EX", "EX/MA", "MA/RW"];

  // Register file geometry: under OF1 and OF2
  const rfX = sx(1) + 10;
  const rfW = sx(2) + SW - 10 - rfX;
  const port1X = slotX(1, "regVal");
  const port2X = slotX(2, "regVal");
  const writePortX = sx(2) + SW - 30;

  const controllerText = [
    b.inProgress ? `beat ${b.number} · µcycle ${b.microcycle}` : `before beat ${b.number}`,
    stallAt >= 0 ? `stall: IF..${b.stallStage} wait, bubble into ${STAGE_NAMES[stallAt + 1]}` : "no stall",
    branch ? `flush: taken branch → ${hex(b.branchPc)}` : "",
    b.inProgress && !b.fetching && stallAt < 0 ? "fetch stopped" : "",
  ]
    .filter((t) => t !== "")
    .join("   ·   ");

  return (
    <svg viewBox={`0 0 ${W} ${H}`} className={"dp" + (blackBox ? " blackbox" : "")}>
      <defs>
        {["data", "feedback", "control"].map((k) => (
          <marker key={k} id={"head-" + k} viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto">
            <path d="M0 0 L10 5 L0 10 z" className={"head head-" + k} />
          </marker>
        ))}
      </defs>

      {/* beat controller and interlock unit (Fig 10.17's Data-lock / Branch-lock units) */}
      <g className={"controller" + (stallAt >= 0 || branch ? " on" : "")}>
        <rect x={sx(0)} y={CTRL_Y} width={W - 2 * LEFT} height={CTRL_H} />
        <text x={sx(0) + 12} y={CTRL_Y + 17} className="unit-title left">
          beat controller · interlock unit
        </text>
        <text x={sx(0) + 12} y={CTRL_Y + 32} className="unit-line left">
          {controllerText}
        </text>
      </g>
      {latches.map((_, k) => {
        const hold = stallAt >= 0 && k < stallAt;
        const bubble = stallAt >= 0 && k === stallAt;
        const flush = branch && k <= 2;
        let kind = "control";
        if (flush) {
          kind = "flush";
        } else if (hold || bubble) {
          kind = "stall";
        }
        const x = latchX(k) + 9;
        return (
          <g key={k}>
            <Wire d={`M${x} ${CTRL_Y + CTRL_H} L${x} ${STAGE_Y - 4}`} on={hold || bubble || flush} kind={kind} flow={false} />
            {(hold || bubble || flush) && (
              <text x={x + 5} y={CTRL_Y + CTRL_H + 14} className={"wire-label " + kind}>
                {flush ? "flush" : hold ? "hold" : "bubble"}
              </text>
            )}
          </g>
        );
      })}

      {/* µcontrol units */}
      {STAGE_NAMES.map((name, i) => {
        const st = stage(i);
        const words = program.stages[i].words;
        const word = words[st.ran ? st.lastUpc : st.upc];
        const waiting = i <= stallAt;
        let status = st.done ? "done" : "running";
        if (waiting) {
          status = "waiting";
        }
        if (st.ran) {
          status = `ran µ${st.lastUpc}`;
        }
        let where = "";
        if (word) {
          where = word.sourceLine > 0 ? `line ${word.sourceLine}${word.part ? " · part" : ""}` : "end of section";
        }
        const x = sx(i) + 22;
        const w = SW - 44;
        return (
          <g key={name}>
            <g className={"ucontrol" + (st.ran ? " on" : "") + (waiting ? " wait" : "")}>
              <rect x={x} y={UC_Y} width={w} height={UC_H} />
              <text x={x + 8} y={UC_Y + 15} className="unit-title left">
                µcontrol {name}
              </text>
              <text x={x + w - 8} y={UC_Y + 15} className="unit-line right">
                µpc {st.upc}
              </text>
              <text x={x + 8} y={UC_Y + 31} className="unit-line left">
                {status} · {where}
              </text>
              <text x={x + 8} y={UC_Y + 47} className="unit-line left code">
                {word ? word.text.slice(0, 27) : ""}
              </text>
            </g>
            {/* control: µcontrol drives its stage's bus (the double arrow of Fig 9.22) */}
            <Wire d={`M${sx(i) + 46} ${UC_Y + UC_H} L${sx(i) + 46} ${STAGE_Y}`} on={st.ran} kind="control" flow={false} />
            {!blackBox && <Wire d={`M${sx(i) + 46} ${STAGE_Y + 52} L${sx(i) + 46} ${BUS_Y}`} on={st.ran} kind="control" flow={false} />}
          </g>
        );
      })}

      {/* stages */}
      {STAGE_NAMES.map((name, i) => {
        const st = stage(i);
        const p = stagePacket(state, i);
        const waiting = i <= stallAt;
        const fault = !p.bubble && p.hasException;
        const slots = slotsFor(state, i, p);
        const geometry = slotGeometry(i, slots.length);
        const slotW = geometry.w;
        const slotX0 = geometry.x0;
        const bus = busText(st);
        let frame = "stage";
        if (st.ran) {
          frame += " on";
        }
        if (waiting) {
          frame += " wait";
        }
        if (fault) {
          frame += " fault";
        }
        if (blackBox) {
          const running = b.inProgress && st.ran; // between beats nothing is running
          const empty = p.bubble && !(i === 0 && b.inProgress && b.fetching);
          const word = program.stages[i].words[st.lastUpc];
          let status = "idle";
          if (waiting) {
            status = "waiting";
          } else if (running) {
            status = "running";
          } else if (!p.bubble) {
            status = "done";
          }
          return (
            <g key={name} className={`box s-${name}` + (running ? " on" : "") + (waiting ? " wait" : "") + (empty ? " empty" : "")}>
              <rect x={sx(i)} y={STAGE_Y} width={SW} height={STAGE_H} />
              <text x={sx(i) + 12} y={STAGE_Y + 24} className="box-name">
                {name}
              </text>
              <text x={sx(i) + SW - 12} y={STAGE_Y + 24} textAnchor="end" className="box-line">
                {status}
              </text>
              <text x={sx(i) + 12} y={STAGE_Y + 44} className="box-line">
                µpc {st.upc}
              </text>
              {i === 0 && !b.inProgress && state.status === "running" && (
                <text x={sx(i) + 12} y={TOKEN_Y + 25} className="box-line">
                  next: fetch {hex(state.units.fetch.pc)}
                </text>
              )}
              <text x={sx(i) + 12} y={STAGE_Y + STAGE_H - 16} className="box-line">
                {running && word ? `µ${st.lastUpc}  ${word.text}`.slice(0, 31) : ""}
              </text>
            </g>
          );
        }
        return (
          <g key={name}>
            <rect x={sx(i)} y={STAGE_Y} width={SW} height={STAGE_H} className={frame} />
            <text x={sx(i) + 10} y={STAGE_Y + 20} className="stage-name">
              {name}
            </text>
            <text x={sx(i) + SW - 10} y={STAGE_Y + 20} textAnchor="end" className="stage-tag">
              {waiting ? "waiting" : st.ran ? "running" : p.bubble && i > 0 ? "idle" : st.done ? "done" : ""}
            </text>
            <text x={sx(i) + 10} y={STAGE_Y + 42} className={"stage-instr" + (p.bubble && i > 0 ? " dim" : "")}>
              {instructionText(state, i, p)}
            </text>

            {/* local bus */}
            <g className={"bus" + (bus ? " on" : "")}>
              <rect x={sx(i) + 10} y={BUS_Y} width={SW - 20} height={BUS_H} />
              <text x={sx(i) + SW / 2} y={BUS_Y + 12} textAnchor="middle" className="bus-label">
                {name} bus
              </text>
            </g>
            <text x={sx(i) + SW / 2} y={BUS_Y - 8} textAnchor="middle" className="bus-text">
              {bus}
            </text>

            {/* registers on the bus */}
            {slots.map((spec, k) => {
              const x = slotX0 + k * (slotW + 6);
              const read = st.ran && st.signals.transfer && st.signals.src === spec.name;
              const written = st.ran && st.signals.transfer && st.signals.dest === spec.name;
              const cx = x + slotW / 2;
              return (
                <g key={spec.name}>
                  {read && <Wire d={`M${cx} ${SLOT_Y} L${cx} ${BUS_Y + BUS_H}`} on kind="data" />}
                  {written && <Wire d={`M${cx} ${BUS_Y + BUS_H} L${cx} ${SLOT_Y}`} on kind="data" />}
                  {!read && !written && <Wire d={`M${cx} ${SLOT_Y} L${cx} ${BUS_Y + BUS_H}`} on={false} kind="data" />}
                  <Slot x={x} w={slotW} spec={spec} read={read} written={written} />
                </g>
              );
            })}

            {/* the stage's functional units */}
            {i === 1 && (
              <Unit
                x={sx(i) + 10}
                y={UNIT_Y}
                w={140}
                h={56}
                title="decode unit"
                lines={[
                  `I ${state.units.decode.I ? 1 : 0} rd ${state.units.decode.rd} rs1 ${state.units.decode.rs1} rs2 ${state.units.decode.rs2}`,
                  `immx ${short(state.units.decode.immx)}`,
                ]}
                on={ran(1) && st.signals.decode}
              />
            )}
            {i === 3 && (
              <g>
                <Unit x={sx(i) + 14} y={UNIT_Y} w={92} h={56} title="ALU" lines={[aluOn ? `<${args(3)}>` : "", short(state.units.alu.aluResult)]} on={aluOn} />
                <g className={"flags" + (aluOn && args(3) === "cmp" ? " on" : "")}>
                  <rect x={sx(i) + 116} y={UNIT_Y} width={84} height={24} />
                  <text x={sx(i) + 158} y={UNIT_Y + 16} textAnchor="middle" className="unit-line">
                    E {state.units.alu.flagE ? 1 : 0} · GT {state.units.alu.flagGT ? 1 : 0}
                  </text>
                </g>
                <Unit
                  x={sx(i) + 116}
                  y={UNIT_Y + 30}
                  w={84}
                  h={26}
                  title={branch ? "branch: taken" : "branch unit"}
                  on={branch}
                  kind="unit branchunit"
                />
              </g>
            )}
          </g>
        );
      })}

      {/* black box: the instructions, sliding from stage to stage */}
      {blackBox &&
        STAGE_NAMES.map((_, i) => {
          const p = stagePacket(state, i);
          if (p.bubble || p.id === 0) {
            return null;
          }
          let cls = "token";
          if (i <= stallAt) {
            cls += " wait";
          }
          if (p.hasException) {
            cls += " fault";
          }
          if (b.inProgress && stage(i).ran) {
            cls += " on";
          }
          return (
            <g key={"token" + p.id} className={cls} style={{ transform: `translate(${sx(i) + 10}px, ${TOKEN_Y}px)` }}>
              <rect width={SW - 20} height={TOKEN_H} />
              <text x={10} y={16} className="token-id">
                #{p.id} · {hex(p.currentPc)}
              </text>
              <text x={10} y={31} className="token-text">
                {tokenText(p)}
              </text>
            </g>
          );
        })}
      {blackBox &&
        ghosts.map((g) => (
          <g key={"ghost" + g.id} transform={`translate(${sx(g.stage) + 10} ${TOKEN_Y})`}>
            <g className={"token ghost " + g.kind}>
              <rect width={SW - 20} height={TOKEN_H} />
              <text x={10} y={16} className="token-id">
                #{g.id} · {g.kind}
              </text>
              <text x={10} y={31} className="token-text">
                {g.text}
              </text>
            </g>
          </g>
        ))}

      {/* pipeline latches */}
      {latches.map((p, k) => {
        const x = latchX(k);
        const full = !p.bubble;
        return (
          <g key={k} className={"latch" + (full ? " full" : "") + (p.hasException ? " fault" : "")}>
            <rect key={p.id} x={x} y={STAGE_Y} width={18} height={STAGE_H} className={full ? "pulse" : ""}>
              <title>{full ? `#${p.id} ${p.assembly}` : "bubble"}</title>
            </rect>
            <text x={x + 9} y={STAGE_Y + STAGE_H / 2} textAnchor="middle" className="latch-name" transform={`rotate(-90 ${x + 9} ${STAGE_Y + STAGE_H / 2})`}>
              {latchNames[k]}
            </text>
            <text x={x + 9} y={STAGE_Y + STAGE_H + 14} textAnchor="middle" className="latch-id">
              {full ? "#" + p.id : "–"}
            </text>
            {/* the packet moves on through the latch */}
            <Wire d={`M${sx(k) + SW} ${STAGE_Y + 60} L${x} ${STAGE_Y + 60}`} on={false} kind="data" />
            <Wire d={`M${x + 18} ${STAGE_Y + 60} L${sx(k + 1)} ${STAGE_Y + 60}`} on={false} kind="data" />
          </g>
        );
      })}

      {/* branch: EX's branch unit sends the target back to IF's pc (red, like Fig 10.17) */}
      <Wire
        d={`M${sx(3) + 158} ${STAGE_Y} L${sx(3) + 158} ${BRANCH_Y} L${sx(0) + SW - 84} ${BRANCH_Y} L${sx(0) + SW - 84} ${STAGE_Y}`}
        on={branch}
        kind="feedback"
      />
      <text x={sx(1) + 20} y={BRANCH_Y - 5} className={"wire-label" + (branch ? " feedback" : "")}>
        branch target → pc{branch ? ` (${hex(b.branchPc)})` : ""}
      </text>

      {/* instruction memory under IF */}
      <Wire d={`M${slotX(0, "pc")} ${portY} L${slotX(0, "pc")} ${MEM_Y}`} on={memFetch} kind="data" />
      <Wire d={`M${slotX(0, "ir")} ${MEM_Y} L${slotX(0, "ir")} ${portY}`} on={memFetch} kind="data" />
      <Unit
        x={sx(0) + 10}
        y={MEM_Y}
        w={SW - 20}
        h={MEM_H}
        title="instruction memory"
        lines={[
          "0x0000 – 0x3FFF",
          memFetch ? `read ${hex(state.units.fetch.pc)}` : "",
          memFetch ? hex(state.units.fetch.ir, 8) : "",
        ]}
        on={memFetch}
        kind="memory"
      />

      {/* register file under OF1 / OF2: two read sides and a write side */}
      <Wire d={`M${port1X} ${portY} L${port1X} ${MEM_Y}`} on={read1} kind="data" />
      <Wire d={`M${port2X} ${portY} L${port2X} ${MEM_Y}`} on={read2} kind="data" />
      <g className={"memory" + (read1 || read2 || write ? " on" : "")}>
        <rect x={rfX} y={MEM_Y} width={rfW} height={MEM_H} />
        <text x={rfX + rfW / 2} y={MEM_Y + 16} textAnchor="middle" className="unit-title">
          register file · read side 1 · read side 2 · write side
        </text>
        {state.registers.map((v, r) => {
          const col = r % 4;
          const row = Math.floor(r / 4);
          const x = rfX + 12 + col * ((rfW - 24) / 4);
          const y = MEM_Y + 34 + row * 14;
          const hot =
            (read1 && r === state.units.registerFile.readSrc1) ||
            (read2 && r === state.units.registerFile.readSrc2) ||
            (write && r === state.units.registerFile.writeSrc);
          return (
            <text key={r} x={x} y={y} className={"reg" + (hot ? " hot" : "")}>
              {`r${r}`.padEnd(4)} {hex(v, 8)}
            </text>
          );
        })}
      </g>

      {/* data memory under MA */}
      <Wire d={`M${slotX(4, "mar")} ${portY} L${slotX(4, "mar")} ${MEM_Y}`} on={memData} kind="data" />
      <Wire d={`M${slotX(4, "mdr")} ${portY} L${slotX(4, "mdr")} ${MEM_Y}`} on={memData && args(4) === "store"} kind="data" />
      <Wire d={`M${slotX(4, "ldResult")} ${MEM_Y} L${slotX(4, "ldResult")} ${portY}`} on={memData && args(4) === "load"} kind="data" />
      <Unit
        x={sx(4) + 10}
        y={MEM_Y}
        w={SW - 20}
        h={MEM_H}
        title="data memory"
        lines={[
          "0x4000 – 0xFFFF",
          memData ? `${args(4)} ${hex(state.units.memoryUnit.mar)}` : "",
          memData ? hex(args(4) === "load" ? state.units.memoryUnit.ldResult : state.units.memoryUnit.mdr, 8) : "",
        ]}
        on={memData}
        kind="memory"
      />
      <text x={sx(4) + SW / 2} y={MEM_Y + MEM_H + 14} textAnchor="middle" className="wire-label">
        same 64 KB memory as IF
      </text>

      {/* write-back: RW writes the register file (red, like Fig 10.17) */}
      <Wire
        d={`M${slotX(5, "regData")} ${portY} L${slotX(5, "regData")} ${WB_Y} L${writePortX} ${WB_Y} L${writePortX} ${MEM_Y + MEM_H}`}
        on={write}
        kind="feedback"
      />
      <text x={sx(3) + 10} y={WB_Y - 5} className={"wire-label" + (write ? " feedback" : "")}>
        write-back{write ? `: r${state.units.registerFile.writeSrc} ← ${short(state.units.registerFile.regData)}` : ""}
      </text>
    </svg>
  );
}
