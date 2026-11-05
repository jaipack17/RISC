import type { PipelineRun } from "../../types";
import { hex } from "../../types";

// The space-time diagram of a run's first beats: one row per instruction, one column per beat,
// each cell the stage the instruction was in. Under each beat number, how many microcycles it took.
export default function SpaceTime({ run }: { run: PipelineRun }) {
  const timeline = run.timeline;
  if (!timeline) {
    return null;
  }
  const names = run.stages.map((s) => s.name);
  const last = timeline.beats.length;
  const recordedAll = last === run.beats;

  return (
    <div className="pipeline-scroll">
      <table className="pipeline spacetime">
        <thead>
          <tr>
            <th className="left">#</th>
            <th className="left">instruction</th>
            {timeline.beats.map((b) => (
              <th key={b.beat} className={b.taken ? "taken" : ""} title={b.taken ? "branch taken in this beat" : ""}>
                {b.beat}
              </th>
            ))}
          </tr>
          <tr className="beat-cycles">
            <th />
            <th className="left dim">µcycles</th>
            {timeline.beats.map((b) => (
              <th key={b.beat} className={"dim" + (b.stall >= 0 ? " stalled" : "")}>
                {b.microcycles}
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {timeline.rows.map((row) => {
            const cells = new Map(row.cells.map(([beat, stage, waited]) => [beat, { stage, waited }]));
            const furthest = Math.max(...row.cells.map((c) => c[1]));
            const lastBeat = Math.max(...row.cells.map((c) => c[0]));
            const flushed = furthest < names.length - 1 && lastBeat < last;
            return (
              <tr key={row.id} className={flushed ? "flushed" : ""}>
                <td className="left dim">{row.id}</td>
                <td className="left mono">
                  {hex(row.pc)} {row.assembly}
                  {flushed && <span className="tag flush-tag">flushed</span>}
                </td>
                {timeline.beats.map((b) => {
                  const cell = cells.get(b.beat);
                  if (!cell) {
                    return <td key={b.beat} />;
                  }
                  return (
                    <td key={b.beat} className={"cell stage-" + names[cell.stage] + (cell.waited ? " waited" : "")}>
                      {names[cell.stage]}
                    </td>
                  );
                })}
              </tr>
            );
          })}
        </tbody>
      </table>
      {!recordedAll && (
        <p className="dim small">
          First {last} of {run.beats} beats.
        </p>
      )}
    </div>
  );
}
