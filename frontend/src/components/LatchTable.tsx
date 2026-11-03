import type { CpuState, Packet } from "../types";
import { hex } from "../types";

// The instruction packet in every latch, field by field
const COLUMNS: [string, (s: CpuState) => Packet][] = [
  ["IF (building)", (s) => s.latches.fetched],
  ["IF/OF1", (s) => s.latches.ifOf1],
  ["OF1/OF2", (s) => s.latches.of1Of2],
  ["OF2/EX", (s) => s.latches.of2Ex],
  ["EX/MA", (s) => s.latches.exMa],
  ["MA/RW", (s) => s.latches.maRw],
];

const FIELDS: [string, (p: Packet) => string][] = [
  ["instruction", (p) => (p.hasException ? p.exception : p.assembly)],
  ["currentPc", (p) => hex(p.currentPc)],
  ["nextPc", (p) => hex(p.nextPc)],
  ["ir", (p) => hex(p.ir, 8)],
  ["rd rs1 rs2", (p) => `${p.rd} ${p.rs1} ${p.rs2}`],
  ["immx", (p) => hex(p.immx, 8)],
  ["branchTarget", (p) => hex(p.branchTarget)],
  ["A", (p) => hex(p.A, 8)],
  ["B", (p) => hex(p.B, 8)],
  ["regVal", (p) => hex(p.regVal, 8)],
  ["regData", (p) => hex(p.regData, 8)],
  ["aluResult", (p) => hex(p.aluResult, 8)],
  ["ldResult", (p) => hex(p.ldResult, 8)],
];

export default function LatchTable({ state }: { state: CpuState }) {
  const packets = COLUMNS.map(([, get]) => get(state));
  return (
    <div className="pipeline-scroll">
      <table className="latches">
        <thead>
          <tr>
            <th className="left" />
            {COLUMNS.map(([name], i) => (
              <th key={name}>
                {name}
                <div className="dim small">{packets[i].bubble ? "bubble" : "#" + packets[i].id}</div>
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {FIELDS.map(([name, show]) => (
            <tr key={name}>
              <td className="left dim">{name}</td>
              {packets.map((p, i) => (
                <td key={i} className={"mono" + (p.bubble ? " dim" : "")}>
                  {p.bubble ? "" : show(p)}
                </td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
