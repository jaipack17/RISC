import type { CpuState, ProgramInfo } from "../types";
import { hex, stageNames, stagePacket } from "../types";

// The assembly program, with the stages each instruction is in right now
export default function ProgramListing({ program, state }: { program: ProgramInfo; state: CpuState }) {
  const names = stageNames(state.processor);
  const stagesAt = new Map<number, string[]>();
  for (let stage = 0; stage < names.length; stage++) {
    const p = stagePacket(state, stage);
    if (!p.bubble) {
      const list = stagesAt.get(p.currentPc) ?? [];
      list.push(names[stage]);
      stagesAt.set(p.currentPc, list);
    }
  }

  return (
    <div className="scroll">
      <table className="listing">
        <tbody>
          {program.listing.map((line, i) => {
            const stages = line.hasInstruction ? stagesAt.get(line.address) ?? [] : [];
            return (
              <tr key={i} className={stages.length > 0 ? "current" : ""}>
                <td className="dim">{line.hasInstruction ? hex(line.address) : ""}</td>
                <td className="dim">{line.hasInstruction ? hex(line.word, 8) : ""}</td>
                <td className={"left mono" + (line.hasInstruction ? " indent" : "")}>
                  {line.text}
                  {line.macro && <span className="tag macro-tag">{line.macro}</span>}
                </td>
                <td className="left">
                  {stages.map((s) => (
                    <span key={s} className={"tag stage-" + s}>
                      {s}
                    </span>
                  ))}
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}
