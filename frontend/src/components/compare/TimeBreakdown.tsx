import { useState } from "react";

// The cycle stack (a CPI stack, in µcycles per instruction). Every beat finishes at most one
// instruction, out of the last stage. A beat that finishes one counts as work. A beat whose last
// stage holds a bubble is charged to why that bubble is there: the interlock put it in behind a
// waiting instruction (stall), or a taken branch threw the slot away (flush). The few empty beats
// while the pipeline fills at the start and empties after hlt are counted with flush, so the
// three parts add up to the total.

export interface Totals {
  instructions: number;
  microcycles: number;
  beats: number;
  stack: { work: number; stall: number; flush: number }; // µcycles
  stackBeats: { work: number; stall: number; flush: number };
}

const PARTS = [
  { key: "work", css: "work", name: "work", about: "beats in which an instruction finished" },
  { key: "stall", css: "stall", name: "stall", about: "beats lost to bubbles the interlock inserted (waiting for a register)" },
  { key: "flush", css: "flush", name: "flush", about: "beats lost to slots a taken branch flushed (and the pipeline filling at the start)" },
] as const;

export function BreakdownLegend() {
  return (
    <div className="legend">
      {PARTS.map((p) => (
        <span key={p.key}>
          <i className={"seg-" + p.css} />
          {p.name}
        </span>
      ))}
    </div>
  );
}

interface Tip {
  x: number;
  y: number;
  part: number;
  row: number;
}

const LABEL_MIN = 7; // % of the widest bar needed to print a value inside its segment

export default function TimeBreakdown({ rows }: { rows: { name: string; totals: Totals }[] }) {
  const [tip, setTip] = useState<Tip | null>(null);
  const perInstruction = (t: Totals, part: number) => t.stack[PARTS[part].key] / Math.max(1, t.instructions);
  const total = (t: Totals) => t.microcycles / Math.max(1, t.instructions);
  const widest = Math.max(...rows.map((r) => total(r.totals)));

  return (
    <div className="breakdown" onPointerLeave={() => setTip(null)}>
      <div className="breakdown-head">
        <span />
        <span>µcycles per instruction</span>
        <span className="num">total</span>
      </div>
      {rows.map((r, row) => (
        <div key={r.name} className="breakdown-row">
          <span className="breakdown-name">{r.name}</span>
          <span className="bar-track" style={{ width: `${(100 * total(r.totals)) / widest}%` }}>
            {PARTS.map((p, part) => {
              const value = perInstruction(r.totals, part);
              const share = (100 * value) / widest;
              return (
                <span
                  key={p.key}
                  className={"seg seg-" + p.css}
                  style={{ width: `${(100 * value) / total(r.totals)}%` }}
                  onPointerMove={(e) => {
                    const box = (e.currentTarget.closest(".breakdown") as HTMLElement).getBoundingClientRect();
                    setTip({ x: e.clientX - box.left + 12, y: e.clientY - box.top + 14, part, row });
                  }}
                >
                  {share >= LABEL_MIN && value.toFixed(2)}
                </span>
              );
            })}
          </span>
          <span className="num strong">{total(r.totals).toFixed(2)}</span>
        </div>
      ))}
      {tip && (
        <div className="chart-tip" style={{ left: tip.x, top: tip.y }}>
          <div className="tip-row">
            <i className={"seg-" + PARTS[tip.part].css} />
            <b>{perInstruction(rows[tip.row].totals, tip.part).toFixed(2)}</b> µcycles per instruction
          </div>
          <div className="tip-note">
            {rows[tip.row].name}, {PARTS[tip.part].name}: {rows[tip.row].totals.stack[PARTS[tip.part].key]} µcycles in{" "}
            {rows[tip.row].totals.stackBeats[PARTS[tip.part].key]} beats
          </div>
          <div className="tip-note">{PARTS[tip.part].about}</div>
        </div>
      )}
    </div>
  );
}
