import { useState } from "react";
import type { CpuState, ProgramInfo } from "../types";
import { hex } from "../types";

interface Props {
  pages: React.ReactNode; // the Simulator | Compare switch
  program: ProgramInfo | null;
  programs: string[];
  state: CpuState | null;
  playing: boolean;
  speed: number;
  onLoad: (name: string) => void;
  onLoadSource: (name: string, source: string) => void;
  onReset: () => void;
  onStepMicrocycle: () => void;
  onStepBeat: () => void;
  onRun: () => void;
  onPlay: () => void;
  onSpeed: (ms: number) => void;
}

export default function Toolbar(props: Props) {
  const { program, programs, state } = props;
  const [editing, setEditing] = useState(false);
  const [source, setSource] = useState("");
  const running = state?.status === "running";

  function statusText(): string {
    if (!state) {
      return "";
    }
    if (state.status === "halted") {
      return "halted";
    }
    if (state.status === "exception" && state.exception) {
      return `exception: ${state.exception.kind} at ${hex(state.exception.pc)}`;
    }
    if (state.beat.inProgress) {
      return `beat ${state.beat.number}, microcycle ${state.beat.microcycle}`;
    }
    return `before beat ${state.beat.number}`;
  }

  return (
    <header className="toolbar">
      <div className="toolbar-row">
        <span className="title">RISC201 · {state?.processor === "four" ? "4" : "6"}-stage microprogrammed pipeline</span>
        <select value={program?.name ?? ""} onChange={(e) => props.onLoad(e.target.value)}>
          {program && !programs.includes(program.name) && <option value={program.name}>{program.name}</option>}
          {programs.map((name) => (
            <option key={name} value={name}>
              {name}
            </option>
          ))}
        </select>
        <button
          onClick={() => {
            setSource(program?.source ?? "");
            setEditing(!editing);
          }}
        >
          {editing ? "Close editor" : "Edit"}
        </button>
        <span className="spacer" />
        <button onClick={props.onReset}>Reset</button>
        <button onClick={props.onStepMicrocycle} disabled={!running}>
          Step µcycle
        </button>
        <button onClick={props.onStepBeat} disabled={!running}>
          Step beat
        </button>
        <button onClick={props.onPlay} disabled={!running}>
          {props.playing ? "Pause" : "Play"}
        </button>
        <label className="speed">
          <input
            type="range"
            min={30}
            max={1000}
            step={10}
            value={1030 - props.speed}
            onChange={(e) => props.onSpeed(1030 - Number(e.target.value))}
          />
        </label>
        <button onClick={props.onRun} disabled={!running}>
          Run to end
        </button>
        {props.pages}
      </div>
      {state && (
        <div className="toolbar-row counters">
          <span className={"status " + state.status}>{statusText()}</span>
          <span>
            µcycles <b>{state.counters.microcycles}</b>
          </span>
          <span>
            beats <b>{state.counters.beats}</b>
          </span>
          <span>
            instructions <b>{state.counters.instructions}</b>
          </span>
          <span>
            stalls <b>{state.counters.stallBeats}</b>
          </span>
          <span>
            taken branches <b>{state.counters.takenBranches}</b>
          </span>
          {state.counters.instructions > 0 && (
            <span>
              µcycles/instr <b>{(state.counters.microcycles / state.counters.instructions).toFixed(2)}</b>
            </span>
          )}
        </div>
      )}
      {editing && (
        <div className="editor">
          <textarea value={source} onChange={(e) => setSource(e.target.value)} spellCheck={false} rows={14} />
          <button
            onClick={() => {
              props.onLoadSource(program?.name ?? "untitled.asm", source);
              setEditing(false);
            }}
          >
            Assemble and load
          </button>
        </div>
      )}
    </header>
  );
}
