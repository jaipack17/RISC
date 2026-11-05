import { useEffect, useState } from "react";
import * as api from "../../api";
import type { CompareResult, PipelineRun } from "../../types";
import SpaceTime from "./SpaceTime";
import SpeedupDots, { speedup } from "./SpeedupDots";
import TimeBreakdown, { BreakdownLegend } from "./TimeBreakdown";
import type { Totals } from "./TimeBreakdown";

// The 4-stage vs 6-stage comparison as a short report: the verdict, where each pipeline's time
// goes, every program's speedup, and one program in detail. All runs are fresh runs of the three
// processors on the backend (GET /api/benchmark); the simulator is not touched.

const CATEGORY_ORDER = ["hazards", "load-use", "alu", "branches", "memory", "stack", "algorithms", "exceptions"];

// The cycle stack in three parts: the beats while the pipeline fills and drains are counted with flush
function totals(runs: PipelineRun[]): Totals {
  const sum = (pick: (r: PipelineRun) => number) => runs.reduce((s, r) => s + pick(r), 0);
  const stack = {
    work: sum((r) => r.cycleStack.work.microcycles),
    stall: sum((r) => r.cycleStack.stall.microcycles),
    flush: sum((r) => r.cycleStack.flush.microcycles + r.cycleStack.fillDrain.microcycles),
  };
  const stackBeats = {
    work: sum((r) => r.cycleStack.work.beats),
    stall: sum((r) => r.cycleStack.stall.beats),
    flush: sum((r) => r.cycleStack.flush.beats + r.cycleStack.fillDrain.beats),
  };
  return {
    instructions: sum((r) => r.instructions),
    microcycles: sum((r) => r.microcycles),
    beats: sum((r) => r.beats),
    stack,
    stackBeats,
  };
}

function ratio(a: number, b: number): string {
  return (a / Math.max(1, b)).toFixed(2);
}

function number(n: number): string {
  return n.toLocaleString("en-US");
}

export default function ComparePage() {
  const asked = new URLSearchParams(window.location.search).get("program") ?? ""; // e.g. ?program=benchmarks/stk11_hanoi.asm
  const [results, setResults] = useState<CompareResult[] | null>(null);
  const [selected, setSelected] = useState(asked);
  const [diagrams, setDiagrams] = useState<CompareResult | null>(null);
  const [showDiagrams, setShowDiagrams] = useState(false);
  const [error, setError] = useState("");

  useEffect(() => {
    api
      .runBenchmark()
      .then((r) => {
        setResults(r);
        if (!asked && r.length > 0) {
          setSelected(r[0].program.name);
        }
      })
      .catch((e) => setError((e as Error).message));
  }, [asked]);

  // The space-time diagrams need the beat-by-beat timeline, which only /api/compare records
  useEffect(() => {
    if (!showDiagrams || !selected || diagrams?.program.name === selected) {
      return;
    }
    api
      .compareProgram(selected)
      .then(setDiagrams)
      .catch((e) => setError((e as Error).message));
  }, [showDiagrams, selected, diagrams]);

  if (error) {
    return <div className="error">{error}</div>;
  }
  if (!results) {
    return <p className="report dim">Running the benchmark programs on both pipelines…</p>;
  }

  const four = totals(results.map((r) => r.four));
  const six = totals(results.map((r) => r.six));
  const sequential = results.reduce((sum, r) => sum + r.sequential.microcycles, 0);
  const faster = results.filter((r) => r.four.microcycles < r.six.microcycles).length;
  const agree = results.filter((r) => r.agree).length;
  const speedups = results.map(speedup);
  const categories = CATEGORY_ORDER.filter((c) => results.some((r) => r.program.category === c));
  const one = results.find((r) => r.program.name === selected) ?? results[0];

  function open(name: string) {
    setSelected(name);
    document.getElementById("one-program")?.scrollIntoView({ behavior: "smooth" });
  }

  return (
    <main className="report">
      <h1>The 4-stage pipeline is {ratio(six.microcycles, four.microcycles)}× faster than the 6-stage one</h1>
      <p className="lead">
        Over {results.length} benchmark programs it needs {number(four.microcycles)} µcycles, the 6-stage pipeline{" "}
        {number(six.microcycles)}, and the sequential microprogrammed processor {number(sequential)}. It is faster on {faster} of{" "}
        {results.length} programs, by {Math.min(...speedups).toFixed(2)}× to {Math.max(...speedups).toFixed(2)}×. Both pipelines run
        the same microcode with no forwarding, and {agree === results.length ? "every program ends" : `${agree} programs end`} in the
        same state on all three processors.
      </p>

      <section>
        <h2>Where the time goes</h2>
        <p>
          Every beat finishes at most one instruction, out of the last stage. A beat that finishes one counts as <b>work</b>. A beat
          whose last stage holds a bubble counts for the reason the bubble is there: a <b>stall</b> (the interlock held an
          instruction back until a register it reads was written, and let a bubble through) or a <b>flush</b> (a taken branch threw
          the slot away). Every µcycle belongs to one beat, so the three parts add up to the total.
        </p>
        <BreakdownLegend />
        <TimeBreakdown
          rows={[
            { name: "4-stage", totals: four },
            { name: "6-stage", totals: six },
          ]}
        />
        <p>
          The difference is the stalls: {ratio(six.stack.stall, six.instructions)} µcycles per instruction on the 6-stage pipeline
          against {ratio(four.stack.stall, four.instructions)}, because a register is written back four stages after it is read
          instead of two. Its beats are shorter ({ratio(six.microcycles, six.beats)} against {ratio(four.microcycles, four.beats)}{" "}
          µcycles), so its work costs a little less ({ratio(six.stack.work, six.instructions)} against{" "}
          {ratio(four.stack.work, four.instructions)}), and flushes cost about the same although it throws away three slots per
          taken branch instead of two.
        </p>
      </section>

      <section>
        <h2>Every program</h2>
        <p>One dot per program: the 6-stage time divided by the 4-stage time. Right of the 1.0× line, the 4-stage pipeline is faster.</p>
        <SpeedupDots results={results} categories={categories} selected={one.program.name} onOpen={open} />
      </section>

      <section id="one-program">
        <h2>One program</h2>
        <select value={one.program.name} onChange={(e) => setSelected(e.target.value)}>
          {categories.map((c) => (
            <optgroup key={c} label={c}>
              {results
                .filter((r) => r.program.category === c)
                .map((r) => (
                  <option key={r.program.name} value={r.program.name}>
                    {r.program.title}
                  </option>
                ))}
            </optgroup>
          ))}
        </select>
        <p>
          {one.program.about} The 4-stage pipeline takes {number(one.four.microcycles)} µcycles, the 6-stage{" "}
          {number(one.six.microcycles)} ({speedup(one).toFixed(2)}×), and the sequential processor {number(one.sequential.microcycles)}.
          {one.four.exception && ` It ends with ${one.four.exception.kind}, as intended.`}
          {!one.agree && ` The processors end in different states:${one.differences}.`}
        </p>
        <TimeBreakdown
          rows={[
            { name: "4-stage", totals: totals([one.four]) },
            { name: "6-stage", totals: totals([one.six]) },
          ]}
        />
        <button className="show" onClick={() => setShowDiagrams(!showDiagrams)}>
          {showDiagrams ? "Hide" : "Show"} the pipeline diagrams
        </button>
        {showDiagrams && diagrams?.program.name === one.program.name && (
          <div className="diagrams">
            <h3>4-stage</h3>
            <SpaceTime run={diagrams.four} />
            <h3>6-stage</h3>
            <SpaceTime run={diagrams.six} />
          </div>
        )}
      </section>
    </main>
  );
}
