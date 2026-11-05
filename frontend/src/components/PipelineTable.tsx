import type { CpuState } from "../types";
import { hex, stageNames, stagePacket } from "../types";

// The classic pipeline diagram: one row per instruction (in the order they were fetched),
// one column per beat, each cell the stage the instruction was in during that beat.

const MAX_BEATS = 60; // only the most recent beats are shown

interface Cell {
  stage: string;
  waited: boolean;
  exception: boolean;
}

interface Row {
  id: number;
  pc: number;
  assembly: string;
  cells: Map<number, Cell>;
  lastStage: number; // the furthest stage reached
}

export default function PipelineTable({ state }: { state: CpuState }) {
  const log = state.beatLog.slice(-MAX_BEATS);
  const names = stageNames(state.processor);
  const rows = new Map<number, Row>();

  function mark(beat: number, stage: number, id: number, pc: number, assembly: string, waited: boolean, exception: boolean) {
    let row = rows.get(id);
    if (!row) {
      row = { id, pc, assembly, cells: new Map(), lastStage: stage };
      rows.set(id, row);
    }
    if (assembly !== "") {
      row.assembly = assembly;
    }
    row.cells.set(beat, { stage: names[stage], waited, exception });
    row.lastStage = Math.max(row.lastStage, stage);
  }

  for (const record of log) {
    record.cells.forEach((cell, stage) => {
      if (!cell.empty && cell.id > 0) {
        mark(record.beat, stage, cell.id, cell.pc, cell.assembly, cell.waited, cell.exception);
      }
    });
  }

  // The beat in progress: what each stage holds right now
  const liveBeat = state.beat.inProgress ? state.beat.number : 0;
  const inPipeline = new Set<number>();
  for (let stage = 0; stage < names.length; stage++) {
    const p = stagePacket(state, stage);
    if (!p.bubble && p.id > 0) {
      inPipeline.add(p.id);
      if (liveBeat > 0) {
        const waited = state.beat.stall && stage <= names.indexOf(state.beat.stallStage);
        mark(liveBeat, stage, p.id, p.currentPc, p.assembly, waited, p.hasException);
      }
    }
  }

  const beats = log.map((record) => record.beat);
  if (liveBeat > 0) {
    beats.push(liveBeat);
  }
  const taken = new Set(log.filter((r) => r.taken).map((r) => r.beat));
  const sorted = [...rows.values()].sort((a, b) => a.id - b.id);

  if (sorted.length === 0) {
    return <p className="dim">Nothing has run yet. Step a microcycle or a beat.</p>;
  }

  return (
    <div className="pipeline-scroll">
      <table className="pipeline">
        <thead>
          <tr>
            <th className="left">#</th>
            <th className="left">instruction</th>
            {beats.map((b) => (
              <th key={b} className={(taken.has(b) ? "taken" : "") + (b === liveBeat ? " live" : "")} title={taken.has(b) ? "branch taken in this beat" : ""}>
                {b}
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {sorted.map((row) => {
            // Flushed: left the pipeline without reaching the last stage
            const flushed = !inPipeline.has(row.id) && row.lastStage < names.length - 1;
            return (
              <tr key={row.id} className={flushed ? "flushed" : ""}>
                <td className="left dim">{row.id}</td>
                <td className="left mono">
                  {hex(row.pc)} {row.assembly}
                  {flushed && <span className="tag flush-tag">flushed</span>}
                </td>
                {beats.map((b) => {
                  const cell = row.cells.get(b);
                  if (!cell) {
                    return <td key={b} />;
                  }
                  let cls = "cell stage-" + cell.stage;
                  if (cell.waited) {
                    cls += " waited";
                  }
                  if (cell.exception) {
                    cls += " fault";
                  }
                  return (
                    <td key={b} className={cls} title={cell.waited ? "waiting (data hazard)" : ""}>
                      {cell.stage}
                    </td>
                  );
                })}
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}
