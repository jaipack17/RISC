import { useState } from "react";
import type { CompareResult } from "../../types";

// One dot per program: how many times longer the 6-stage pipeline takes than the 4-stage one
// (6-stage µcycles ÷ 4-stage µcycles). Right of the 1.0 line, the 4-stage pipeline is faster.
// A row per category; click a dot to look at that program.

const W = 900;
const LABEL_W = 130;
const ROW_H = 34;
const TOP = 10;
const AXIS_H = 26;

export function speedup(r: CompareResult): number {
  return r.six.microcycles / Math.max(1, r.four.microcycles);
}

interface Tip {
  x: number;
  y: number;
  r: CompareResult;
}

export default function SpeedupDots({
  results,
  categories,
  selected,
  onOpen,
}: {
  results: CompareResult[];
  categories: string[];
  selected: string;
  onOpen: (name: string) => void;
}) {
  const [tip, setTip] = useState<Tip | null>(null);
  const values = results.map(speedup);
  const lo = Math.min(1, Math.floor(Math.min(...values) * 10) / 10);
  const hi = Math.max(lo + 0.1, Math.ceil(Math.max(...values) * 10) / 10);
  const x = (v: number) => LABEL_W + ((W - LABEL_W - 20) * (v - lo)) / (hi - lo);
  const ticks: number[] = [];
  for (let t = lo; t <= hi + 1e-9; t += 0.1) {
    ticks.push(Math.round(t * 10) / 10);
  }
  const height = TOP + categories.length * ROW_H + AXIS_H;

  return (
    <div className="dots" onPointerLeave={() => setTip(null)}>
      <svg width="100%" viewBox={`0 0 ${W} ${height}`}>
        {ticks.map((t) => (
          <g key={t}>
            <line x1={x(t)} x2={x(t)} y1={TOP - 4} y2={height - AXIS_H + 4} className={t === 1 ? "reference" : "grid"} />
            <text x={x(t)} y={height - 8} textAnchor="middle" className="axis">
              {t.toFixed(1)}×
            </text>
          </g>
        ))}
        {categories.map((c, row) => {
          const y = TOP + row * ROW_H + ROW_H / 2;
          const inRow = results.filter((r) => r.program.category === c).sort((a, b) => speedup(a) - speedup(b));
          return (
            <g key={c}>
              <line x1={LABEL_W} x2={W - 20} y1={y + ROW_H / 2} y2={y + ROW_H / 2} className="grid row-rule" />
              <text x={0} y={y + 4} className="row-label">
                {c}
              </text>
              {inRow.map((r, k) => {
                const cx = x(speedup(r));
                const cy = y + [-6, 0, 6][k % 3]; // spread neighbours so close dots stay visible
                const isSelected = r.program.name === selected;
                return (
                  <g
                    key={r.program.name}
                    className={"dot" + (isSelected ? " selected" : "")}
                    onClick={() => onOpen(r.program.name)}
                    onPointerMove={(e) => {
                      const box = (e.currentTarget.ownerSVGElement as SVGSVGElement).getBoundingClientRect();
                      setTip({ x: e.clientX - box.left + 12, y: e.clientY - box.top + 14, r });
                    }}
                  >
                    <circle cx={cx} cy={cy} r={12} className="hit" />
                    <circle cx={cx} cy={cy} r={isSelected ? 5.5 : 4} className="mark" />
                  </g>
                );
              })}
            </g>
          );
        })}
      </svg>
      {tip && (
        <div className="chart-tip" style={{ left: tip.x, top: tip.y }}>
          <div className="tip-row">
            <b>{speedup(tip.r).toFixed(2)}×</b> {tip.r.program.title}
          </div>
          <div className="tip-note">
            4-stage {tip.r.four.microcycles} µcycles · 6-stage {tip.r.six.microcycles} µcycles
          </div>
          <div className="tip-note">click to look at it below</div>
        </div>
      )}
    </div>
  );
}
