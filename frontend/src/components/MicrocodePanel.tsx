import { useEffect, useRef, useState } from "react";
import type { CpuState, ProgramInfo } from "../types";
import { stageNames, stagePacket } from "../types";

// One stage's microprogram (the sections the splitter made from vertical.masm),
// with the microinstruction it just ran and the one its µpc points at
export default function MicrocodePanel({ program, state }: { program: ProgramInfo; state: CpuState }) {
  const [chosen, setStage] = useState(1);
  const names = stageNames(state.processor);
  const stage = Math.min(chosen, names.length - 1); // the four-stage processor has fewer tabs
  const current = useRef<HTMLTableRowElement | null>(null);
  const box = useRef<HTMLDivElement | null>(null);
  const st = state.stages[stage];
  const words = program.stages[stage].words;
  const packet = stagePacket(state, stage);

  // Which instruction's section the µpc is in
  let section = "";
  if (!packet.bubble && stage > 0) {
    section = packet.assembly.split(" ")[0];
  }

  // Keep the highlighted row in view (scrolls only the table, not the page)
  useEffect(() => {
    const row = current.current;
    const container = box.current;
    if (row && container) {
      container.scrollTop = row.offsetTop - container.clientHeight / 2;
    }
  }, [st.upc, st.lastUpc, stage]);

  return (
    <div>
      <div className="tabs">
        {names.map((name, i) => (
          <button key={name} className={i === stage ? "selected" : ""} onClick={() => setStage(i)}>
            {name}
            {state.stages[i].ran && <span className="dot" />}
          </button>
        ))}
        <span className="dim small">
          µpc {st.upc}
          {section && ` · ${section} section`}
        </span>
      </div>
      <div className="scroll" ref={box} style={{ position: "relative" }}>
        <table className="microcode">
          <thead>
            <tr>
              <th>µaddr</th>
              <th>line</th>
              <th className="left">microinstruction</th>
            </tr>
          </thead>
          <tbody>
            {words.map((w) => {
              const ranHere = st.ran && st.lastUpc === w.address;
              const next = !st.done && st.upc === w.address;
              let cls = w.sourceLine === 0 ? "end" : "";
              if (ranHere) {
                cls += " ran";
              } else if (next) {
                cls += " next";
              }
              return (
                <tr key={w.address} className={cls} ref={ranHere || next ? current : undefined}>
                  <td className="dim">{w.address}</td>
                  <td className="dim">{w.sourceLine || ""}</td>
                  <td className="left mono">
                    {w.text}
                    {w.part && <span className="tag">{names[stage]} part</span>}
                    {ranHere && st.aluWait > 0 && (
                      <span className="tag busy">
                        ALU busy · {st.aluWait} more µcycle{st.aluWait === 1 ? "" : "s"}
                      </span>
                    )}
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
}
