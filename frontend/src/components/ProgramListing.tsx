import type { CpuState, ProgramInfo } from "../types";
import { STAGE_NAMES, hex, stagePacket } from "../types";

// The assembly program, with the stages each instruction is in right now
export default function ProgramListing({ program, state }: { program: ProgramInfo; state: CpuState }) {
  const stagesAt = new Map<number, string[]>();
  for (let stage = 0; stage < STAGE_NAMES.length; stage++) {
    const p = stagePacket(state, stage);
    if (!p.bubble) {
      const list = stagesAt.get(p.currentPc) ?? [];
      list.push(STAGE_NAMES[stage]);
      stagesAt.set(p.currentPc, list);
    }
  }

  return (
    <div className="scroll">
      <table className="listing">
        <tbody>
          {program.listing.map((line) => {
            const stages = line.hasInstruction ? stagesAt.get(line.address) ?? [] : [];
            return (
              <tr key={line.line} className={stages.length > 0 ? "current" : ""}>
                <td className="dim">{line.hasInstruction ? hex(line.address) : ""}</td>
                <td className="dim">{line.hasInstruction ? hex(line.word, 8) : ""}</td>
                <td className={"left mono" + (line.hasInstruction ? " indent" : "")}>{line.text}</td>
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
