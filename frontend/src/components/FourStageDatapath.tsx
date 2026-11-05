import type { CpuState, Packet, ProgramInfo } from "../types";
import { FOUR_STAGE_NAMES, hex, latchList, stagePacket } from "../types";
import type { SlotSpec } from "./diagram";
import {
  BRANCH_Y,
  BUS_H,
  BUS_Y,
  CTRL_H,
  CTRL_Y,
  H,
  MEM_H,
  MEM_Y,
  Markers,
  SLOT_H,
  SLOT_Y,
  STAGE_H,
  STAGE_Y,
  Slot,
  TOKEN_H,
  TOKEN_Y,
  UC_H,
  UC_Y,
  UNIT_Y,
  Unit,
  WB_Y,
  Wire,
  busText,
  instructionText,
  short,
  slotGeometry as placeSlots,
  tokenText,
  useGhosts,
} from "./diagram";

// Block diagram of the four-stage pipelined microprogrammed processor, in the same style as the
// six-stage one: IF | OF | EX | MW, where EX is OF2 + EX (second operand, ALU, branch) and
// MW is MA + RW (data memory, write-back). The register file sits under OF and EX, the data
// memory under MW, and the write-back path runs from MW back to the register file.

const SW = 343; // stage width
const GAP = 44; // space between stages, the latch sits in the middle
const LEFT = 24;
const W = LEFT * 2 + 4 * SW + 3 * GAP;

const IF = 0;
const OF = 1;
const EX = 2;
const MW = 3;

function sx(i: number) {
  return LEFT + i * (SW + GAP);
}

function latchX(i: number) {
  return sx(i) + SW + GAP / 2 - 9;
}

// Where the registers of stage i sit on its bus
function slotGeometry(i: number, count: number) {
  return placeSlots(sx(i), SW, count);
}

// ---- what each stage shows ----

function slotsFor(state: CpuState, i: number, p: Packet): SlotSpec[] {
  const u = state.units;
  const rf = u.registerFile;
  switch (i) {
    case IF:
      return [
        { name: "pc", value: short(u.fetch.pc) },
        { name: "ir", value: hex(u.fetch.ir, 8) },
      ];
    case OF:
      return [
        { name: "A", value: short(p.A) },
        { name: "regSrc", value: "r" + rf.readSrc1 },
        { name: "regVal", value: short(rf.regVal1) },
      ];
    case EX:
      return [
        { name: "regSrc", value: "r" + rf.readSrc2 },
        { name: "regVal", value: short(rf.regVal2) },
        { name: "A", value: short(p.A) },
        { name: "B", value: short(p.B) },
        { name: "regData", value: short(p.regData) },
        { name: "aluResult", label: "aluRes", value: short(u.alu.aluResult) },
      ];
    default:
      return [
        { name: "mar", value: short(u.memoryUnit.mar) },
        { name: "mdr", value: short(u.memoryUnit.mdr) },
        {
          name: "ldResult",
          label: "ldRes",
          value: short(u.memoryUnit.ldResult),
        },
        { name: "regSrc", value: "r" + rf.writeSrc },
        { name: "regData", value: short(rf.regData) },
      ];
  }
}

export default function FourStageDatapath({ state, program, blackBox }: { state: CpuState; program: ProgramInfo; blackBox: boolean }) {
  const b = state.beat;
  const stallAt = b.inProgress && b.stall ? (FOUR_STAGE_NAMES as readonly string[]).indexOf(b.stallStage) : -1;
  const stage = (i: number) => state.stages[i];
  const ran = (i: number) => stage(i).ran;
  const argsUnit = (i: number) => (ran(i) ? stage(i).signals.argsUnit : "");
  const args = (i: number) => (ran(i) ? stage(i).signals.args : "");

  const ghosts = useGhosts(state);

  // Wires to the memories and the register file start at the registers, or at the bottom of a black box
  const portY = blackBox ? STAGE_Y + STAGE_H : SLOT_Y + SLOT_H;

  const memFetch = ran(IF) && stage(IF).signals.loadIR;
  const memData = argsUnit(MW) === "memory";
  const read1 = args(OF) === "read";
  const read2 = args(EX) === "read";
  const write = args(MW) === "write";
  const aluOn = argsUnit(EX) === "alu";
  const branch = b.branchTaken;

  // x of the middle of register `name` in stage i (for the wires to memories and the register file)
  const slotX = (i: number, name: string) => {
    const slots = slotsFor(state, i, stagePacket(state, i));
    const g = slotGeometry(i, slots.length);
    const k = slots.findIndex((spec) => spec.name === name);
    return g.x0 + k * (g.w + 6) + g.w / 2;
  };

  const latches = latchList(state);

  // Register file geometry: under OF and EX
  const rfX = sx(OF) + 10;
  const rfW = sx(EX) + SW - 10 - rfX;
  const port1X = slotX(OF, "regVal");
  const port2X = slotX(EX, "regVal");
  const writePortX = sx(EX) + SW - 30;

  // Data memory under MW's mar, mdr and ldResult, leaving room for the write-back wire
  const dmX = sx(MW) + 10;
  const dmW = slotX(MW, "ldResult") - dmX + 40;

  const controllerText = [
    b.inProgress ? `beat ${b.number} · µcycle ${b.microcycle}` : `before beat ${b.number}`,
    stallAt >= 0 ? `stall: IF..${b.stallStage} wait, bubble into ${FOUR_STAGE_NAMES[stallAt + 1]}` : "no stall",
    branch ? `flush: taken branch → ${hex(b.branchPc)}` : "",
    b.inProgress && !b.fetching && stallAt < 0 ? "fetch stopped" : "",
  ]
    .filter((t) => t !== "")
    .join("   ·   ");

  return (
    <svg viewBox={`0 0 ${W} ${H}`} className={"dp" + (blackBox ? " blackbox" : "")}>
      <Markers />

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
        const flush = branch && k <= 1;
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
      {FOUR_STAGE_NAMES.map((name, i) => {
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
                {word ? word.text.slice(0, 46) : ""}
              </text>
            </g>
            {/* control: µcontrol drives its stage's bus (the double arrow of Fig 9.22) */}
            <Wire d={`M${sx(i) + 46} ${UC_Y + UC_H} L${sx(i) + 46} ${STAGE_Y}`} on={st.ran} kind="control" flow={false} />
            {!blackBox && <Wire d={`M${sx(i) + 46} ${STAGE_Y + 52} L${sx(i) + 46} ${BUS_Y}`} on={st.ran} kind="control" flow={false} />}
          </g>
        );
      })}

      {/* stages */}
      {FOUR_STAGE_NAMES.map((name, i) => {
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
          const empty = p.bubble && !(i === IF && b.inProgress && b.fetching);
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
              {i === IF && !b.inProgress && state.status === "running" && (
                <text x={sx(i) + 12} y={TOKEN_Y + 25} className="box-line">
                  next: fetch {hex(state.units.fetch.pc)}
                </text>
              )}
              <text x={sx(i) + 12} y={STAGE_Y + STAGE_H - 16} className="box-line">
                {running && word ? `µ${st.lastUpc}  ${word.text}`.slice(0, 50) : ""}
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
            {i === OF && (
              <Unit
                x={sx(i) + 10}
                y={UNIT_Y}
                w={170}
                h={56}
                title="decode unit"
                lines={[
                  `I ${state.units.decode.I ? 1 : 0}  rd ${state.units.decode.rd}  rs1 ${state.units.decode.rs1}  rs2 ${state.units.decode.rs2}`,
                  `immx ${short(state.units.decode.immx)}`,
                ]}
                on={ran(OF) && st.signals.decode}
              />
            )}
            {i === EX && (
              <g>
                <Unit
                  x={sx(i) + 14}
                  y={UNIT_Y}
                  w={120}
                  h={56}
                  title="ALU"
                  lines={[aluOn ? `<${args(EX)}>` : "", short(state.units.alu.aluResult)]}
                  on={aluOn}
                />
                <g className={"flags" + (aluOn && args(EX) === "cmp" ? " on" : "")}>
                  <rect x={sx(i) + 146} y={UNIT_Y} width={96} height={24} />
                  <text x={sx(i) + 194} y={UNIT_Y + 16} textAnchor="middle" className="unit-line">
                    E {state.units.alu.flagE ? 1 : 0} · GT {state.units.alu.flagGT ? 1 : 0}
                  </text>
                </g>
                <Unit x={sx(i) + 146} y={UNIT_Y + 30} w={96} h={26} title={branch ? "branch: taken" : "branch unit"} on={branch} kind="unit branchunit" />
              </g>
            )}
          </g>
        );
      })}

      {/* black box: the instructions, sliding from stage to stage */}
      {blackBox &&
        FOUR_STAGE_NAMES.map((_, i) => {
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
      {latches.map(({ name, packet: p }, k) => {
        const x = latchX(k);
        const full = !p.bubble;
        return (
          <g key={k} className={"latch" + (full ? " full" : "") + (p.hasException ? " fault" : "")}>
            <rect key={p.id} x={x} y={STAGE_Y} width={18} height={STAGE_H} className={full ? "pulse" : ""}>
              <title>{full ? `#${p.id} ${p.assembly}` : "bubble"}</title>
            </rect>
            <text x={x + 9} y={STAGE_Y + STAGE_H / 2} textAnchor="middle" className="latch-name" transform={`rotate(-90 ${x + 9} ${STAGE_Y + STAGE_H / 2})`}>
              {name}
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
        d={`M${sx(EX) + 194} ${STAGE_Y} L${sx(EX) + 194} ${BRANCH_Y} L${sx(IF) + SW - 84} ${BRANCH_Y} L${sx(IF) + SW - 84} ${STAGE_Y}`}
        on={branch}
        kind="feedback"
      />
      <text x={sx(OF) + 20} y={BRANCH_Y - 5} className={"wire-label" + (branch ? " feedback" : "")}>
        branch target → pc{branch ? ` (${hex(b.branchPc)})` : ""}
      </text>

      {/* instruction memory under IF */}
      <Wire d={`M${slotX(IF, "pc")} ${portY} L${slotX(IF, "pc")} ${MEM_Y}`} on={memFetch} kind="data" />
      <Wire d={`M${slotX(IF, "ir")} ${MEM_Y} L${slotX(IF, "ir")} ${portY}`} on={memFetch} kind="data" />
      <Unit
        x={sx(IF) + 10}
        y={MEM_Y}
        w={SW - 20}
        h={MEM_H}
        title="instruction memory"
        lines={["0x0000 – 0x3FFF", memFetch ? `read ${hex(state.units.fetch.pc)}` : "", memFetch ? hex(state.units.fetch.ir, 8) : ""]}
        on={memFetch}
        kind="memory"
      />

      {/* register file under OF / EX: two read sides and a write side */}
      <Wire d={`M${port1X} ${portY} L${port1X} ${MEM_Y}`} on={read1} kind="data" />
      <Wire d={`M${port2X} ${portY} L${port2X} ${MEM_Y}`} on={read2} kind="data" />
      <g className={"memory" + (read1 || read2 || write ? " on" : "")}>
        <rect x={rfX} y={MEM_Y} width={rfW} height={MEM_H} />
        <text x={rfX + rfW / 2} y={MEM_Y + 16} textAnchor="middle" className="unit-title">
          register file · read side 1 (OF) · read side 2 (EX) · write side (MW)
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

      {/* data memory under MW */}
      <Wire d={`M${slotX(MW, "mar")} ${portY} L${slotX(MW, "mar")} ${MEM_Y}`} on={memData} kind="data" />
      <Wire d={`M${slotX(MW, "mdr")} ${portY} L${slotX(MW, "mdr")} ${MEM_Y}`} on={memData && args(MW) === "store"} kind="data" />
      <Wire d={`M${slotX(MW, "ldResult")} ${MEM_Y} L${slotX(MW, "ldResult")} ${portY}`} on={memData && args(MW) === "load"} kind="data" />
      <Unit
        x={dmX}
        y={MEM_Y}
        w={dmW}
        h={MEM_H}
        title="data memory"
        lines={[
          "0x4000 – 0xFFFF",
          memData ? `${args(MW)} ${hex(state.units.memoryUnit.mar)}` : "",
          memData ? hex(args(MW) === "load" ? state.units.memoryUnit.ldResult : state.units.memoryUnit.mdr, 8) : "",
        ]}
        on={memData}
        kind="memory"
      />
      <text x={dmX + dmW / 2} y={MEM_Y + MEM_H + 14} textAnchor="middle" className="wire-label">
        same 64 KB memory as IF
      </text>

      {/* write-back: MW writes the register file (red, like Fig 10.17) */}
      <Wire
        d={`M${slotX(MW, "regData")} ${portY} L${slotX(MW, "regData")} ${WB_Y} L${writePortX} ${WB_Y} L${writePortX} ${MEM_Y + MEM_H}`}
        on={write}
        kind="feedback"
      />
      <text x={writePortX + 10} y={WB_Y - 5} className={"wire-label" + (write ? " feedback" : "")}>
        write-back
        {write ? `: r${state.units.registerFile.writeSrc} ← ${short(state.units.registerFile.regData)}` : ""}
      </text>
    </svg>
  );
}
