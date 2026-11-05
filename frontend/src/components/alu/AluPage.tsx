import { useEffect, useState } from "react";
import * as api from "../../api";
import type { AdderKey, AluConfig, AluReport, AluRun, IterativeMeasure } from "../../types";
import { ADDER_NAMES, DIVIDER_NAMES, MULTIPLIER_NAMES } from "../../types";

// The ALU algorithms compared: first each unit on its own (delay, area and µcycles counted from
// the gates it is built of), then what each algorithm does to the 100 benchmarks on the three
// processors. Each run changes one unit and keeps the other two at the default.

type Unit = "adder" | "multiplier" | "divider";
type ProcessorKey = "four" | "six" | "sequential";

const PROCESSORS: { key: ProcessorKey; name: string }[] = [
  { key: "four", name: "4-stage" },
  { key: "six", name: "6-stage" },
  { key: "sequential", name: "sequential" },
];

function number(n: number): string {
  return n.toLocaleString("en-US");
}

function algorithmName(unit: Unit, alu: AluConfig): string {
  if (unit === "adder") {
    return ADDER_NAMES[alu.adder];
  }
  if (unit === "multiplier") {
    return MULTIPLIER_NAMES[alu.multiplier];
  }
  return DIVIDER_NAMES[alu.divider];
}

// The default run first, then the runs that change this unit
function runsOf(report: AluReport, unit: Unit): AluRun[] {
  return report.runs.filter((r) => r.unit === "default" || r.unit === unit);
}

function slowdown(run: AluRun, base: AluRun, processor: ProcessorKey): number {
  return run[processor].microcycles / base[processor].microcycles;
}

function MeasureBar({ value, widest }: { value: number; widest: number }) {
  return (
    <span className="alu-bar">
      <span className="seg seg-work" style={{ width: `${(100 * value) / widest}%` }} />
    </span>
  );
}

function AdderTable({ report }: { report: AluReport }) {
  const units = report.units;
  const widest = Math.max(...units.adders.map((a) => a.delay));
  return (
    <table className="alu-table">
      <thead>
        <tr>
          <th className="left">adder</th>
          <th className="left">delay (gate delays)</th>
          <th>area</th>
          <th>cmp delay</th>
          <th>add µcycles</th>
          <th>cmp µcycles</th>
        </tr>
      </thead>
      <tbody>
        {units.adders.map((a) => (
          <tr key={a.key}>
            <td className="left">
              {a.name}
              {a.key === units.defaults.adder && <span className="tag">default</span>}
            </td>
            <td className="left">
              <MeasureBar value={a.delay} widest={widest} />
              <span className="value">{a.delay}</span>
            </td>
            <td>{a.area}</td>
            <td>{a.compareDelay}</td>
            <td className="strong">{a.addMicrocycles}</td>
            <td className="strong">{a.compareMicrocycles}</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

function IterativeTable({ report, adder }: { report: AluReport; adder: AdderKey }) {
  const units = report.units;
  const rows: { key: string; name: string; isDefault: boolean; m: IterativeMeasure }[] = [];
  for (const m of units.multipliers) {
    rows.push({ key: m.key, name: m.name, isDefault: m.key === units.defaults.multiplier, m: m.byAdder[adder] });
  }
  for (const d of units.dividers) {
    rows.push({ key: d.key, name: d.name + " divider", isDefault: d.key === units.defaults.divider, m: d.byAdder[adder] });
  }
  const widest = Math.max(...rows.map((r) => r.m.max));
  return (
    <table className="alu-table">
      <thead>
        <tr>
          <th className="left">unit</th>
          <th className="left">µcycles per operation (average, and the range)</th>
          <th>step delay</th>
          <th>area</th>
        </tr>
      </thead>
      <tbody>
        {rows.map((r, i) => (
          <tr key={r.key} className={i === units.multipliers.length ? "group-start" : ""}>
            <td className="left">
              {r.name}
              {r.isDefault && <span className="tag">default</span>}
            </td>
            <td className="left">
              <MeasureBar value={r.m.average} widest={widest} />
              <span className="value">
                {r.m.average.toFixed(1)} <span className="dim">({r.m.min === r.m.max ? r.m.min : `${r.m.min}–${r.m.max}`})</span>
              </span>
            </td>
            <td>{r.m.stepDelay}</td>
            <td>{r.m.area}</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

function BenchmarkTable({ report, unit }: { report: AluReport; unit: Unit }) {
  const runs = runsOf(report, unit);
  const base = runs[0];
  const widest: Record<ProcessorKey, number> = { four: 0, six: 0, sequential: 0 };
  for (const p of PROCESSORS) {
    widest[p.key] = Math.max(...runs.map((r) => r[p.key].microcycles));
  }
  return (
    <table className="alu-table runs">
      <thead>
        <tr>
          <th className="left">{unit}</th>
          {PROCESSORS.map((p) => (
            <th key={p.key} className="left">
              {p.name} µcycles
            </th>
          ))}
          <th>6-stage ÷ 4-stage</th>
        </tr>
      </thead>
      <tbody>
        {runs.map((r) => (
          <tr key={JSON.stringify(r.alu)}>
            <td className="left">
              {algorithmName(unit, r.alu)}
              {r.unit === "default" && <span className="tag">default</span>}
            </td>
            {PROCESSORS.map((p) => (
              <td key={p.key} className="left">
                <MeasureBar value={r[p.key].microcycles} widest={widest[p.key]} />
                <span className="value">
                  {number(r[p.key].microcycles)}
                  {r.unit !== "default" && <span className="dim"> {slowdown(r, base, p.key).toFixed(2)}×</span>}
                </span>
              </td>
            ))}
            <td className="strong">{(r.six.microcycles / r.four.microcycles).toFixed(2)}×</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

// For each other algorithm, the program it slows down the most on the 4-stage pipeline
function MostAffected({ report }: { report: AluReport }) {
  const base = report.runs[0];
  return (
    <table className="alu-table">
      <thead>
        <tr>
          <th className="left">instead of the default</th>
          <th className="left">program it slows most (4-stage)</th>
          <th>default µcycles</th>
          <th>µcycles</th>
          <th>slower</th>
        </tr>
      </thead>
      <tbody>
        {report.runs
          .filter((r) => r.unit !== "default")
          .map((r) => {
            let worst = 0;
            r.four.perProgram.forEach((m, i) => {
              if (m / base.four.perProgram[i] > r.four.perProgram[worst] / base.four.perProgram[worst]) {
                worst = i;
              }
            });
            const unit = r.unit as Unit;
            return (
              <tr key={JSON.stringify(r.alu)}>
                <td className="left">
                  {algorithmName(unit, r.alu)} {unit}
                </td>
                <td className="left">{report.programs[worst].title}</td>
                <td>{number(base.four.perProgram[worst])}</td>
                <td>{number(r.four.perProgram[worst])}</td>
                <td className="strong">{(r.four.perProgram[worst] / base.four.perProgram[worst]).toFixed(2)}×</td>
              </tr>
            );
          })}
      </tbody>
    </table>
  );
}

export default function AluPage() {
  const [report, setReport] = useState<AluReport | null>(null);
  const [adder, setAdder] = useState<AdderKey>("prefix");
  const [error, setError] = useState("");

  useEffect(() => {
    api
      .getAluReport()
      .then((r) => {
        setReport(r);
        setAdder(r.units.defaults.adder);
      })
      .catch((e) => setError((e as Error).message));
  }, []);

  if (error) {
    return <div className="error">{error}</div>;
  }
  if (!report) {
    return <p className="report dim">Running the benchmarks once per algorithm…</p>;
  }

  const units = report.units;

  return (
    <main className="report">
      <h1>Comparing Different ALU Algorithms</h1>
      <p className="lead">
        Delays in gate delays (unit-gate model), one µcycle = {units.microcycleLength} gate delays. Benchmark totals over{" "}
        {report.programs.length} programs.
      </p>

      <section>
        <h2>Adders</h2>
        <AdderTable report={report} />
      </section>

      <section>
        <h2>Multipliers and dividers</h2>
        <div className="toggle adder-choice">
          {units.adders.map((a) => (
            <button key={a.key} className={a.key === adder ? "selected" : ""} onClick={() => setAdder(a.key)}>
              {ADDER_NAMES[a.key]}
            </button>
          ))}
        </div>
        <IterativeTable report={report} adder={adder} />
      </section>

      <section>
        <h2>On the benchmarks</h2>
        <h3>adder</h3>
        <BenchmarkTable report={report} unit="adder" />
        <h3>multiplier</h3>
        <BenchmarkTable report={report} unit="multiplier" />
        <h3>divider</h3>
        <BenchmarkTable report={report} unit="divider" />
      </section>

      <section>
        <h2>Most affected program</h2>
        <MostAffected report={report} />
      </section>
    </main>
  );
}
