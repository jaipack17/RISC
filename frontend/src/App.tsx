import { useCallback, useEffect, useRef, useState } from "react";
import * as api from "./api";
import type { CpuState, ProgramInfo } from "./types";
import Toolbar from "./components/Toolbar";
import Datapath from "./components/Datapath";
import PipelineTable from "./components/PipelineTable";
import ProgramListing from "./components/ProgramListing";
import MicrocodePanel from "./components/MicrocodePanel";
import RegistersPanel from "./components/RegistersPanel";
import MemoryPanel from "./components/MemoryPanel";
import LatchTable from "./components/LatchTable";

export default function App() {
  const [program, setProgram] = useState<ProgramInfo | null>(null);
  const [state, setState] = useState<CpuState | null>(null);
  const [previous, setPrevious] = useState<CpuState | null>(null); // to highlight what changed
  const [programs, setPrograms] = useState<string[]>([]);
  const [error, setError] = useState("");
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(300); // ms per microcycle while playing
  const busy = useRef(false);
  const [view, setView] = useState<"detailed" | "blackbox">(() => {
    const asked = new URLSearchParams(window.location.search).get("view"); // e.g. ?view=blackbox
    if (asked === "blackbox" || asked === "detailed") {
      return asked;
    }
    try {
      return localStorage.getItem("view") === "blackbox" ? "blackbox" : "detailed";
    } catch {
      return "detailed";
    }
  });

  function chooseView(next: "detailed" | "blackbox") {
    setView(next);
    try {
      localStorage.setItem("view", next);
    } catch {
      // the choice just isn't remembered
    }
  }

  const show = useCallback((next: CpuState) => {
    setState((current) => {
      setPrevious(current);
      return next;
    });
  }, []);

  // One request at a time, so a fast "play" cannot overtake itself
  const call = useCallback(
    async (action: () => Promise<CpuState>) => {
      if (busy.current) {
        return;
      }
      busy.current = true;
      try {
        show(await action());
        setError("");
      } catch (e) {
        setError((e as Error).message);
        setPlaying(false);
      } finally {
        busy.current = false;
      }
    },
    [show],
  );

  useEffect(() => {
    Promise.all([api.getProgram(), api.getState(), api.listPrograms()])
      .then(([p, s, list]) => {
        setProgram(p);
        setState(s);
        setPrograms(list);
      })
      .catch(() => setError("Cannot reach the backend. Start it with ./build/risc_web from the project folder."));
  }, []);

  // Play: one microcycle every `speed` ms until the program stops
  useEffect(() => {
    if (!playing) {
      return;
    }
    const timer = setInterval(() => call(() => api.step("microcycle")), speed);
    return () => clearInterval(timer);
  }, [playing, speed, call]);

  useEffect(() => {
    if (state && state.status !== "running") {
      setPlaying(false);
    }
  }, [state]);

  async function load(name: string) {
    setPlaying(false);
    try {
      setProgram(await api.loadProgramFile(name));
      const s = await api.getState();
      setPrevious(null);
      setState(s);
      setError("");
    } catch (e) {
      setError((e as Error).message);
    }
  }

  async function loadSource(name: string, source: string) {
    setPlaying(false);
    try {
      setProgram(await api.loadProgramSource(name, source));
      const s = await api.getState();
      setPrevious(null);
      setState(s);
      setError("");
    } catch (e) {
      setError((e as Error).message);
    }
  }

  return (
    <div className="app">
      <Toolbar
        program={program}
        programs={programs}
        state={state}
        playing={playing}
        speed={speed}
        onLoad={load}
        onLoadSource={loadSource}
        onReset={() => {
          setPlaying(false);
          call(api.reset);
        }}
        onStepMicrocycle={() => call(() => api.step("microcycle"))}
        onStepBeat={() => call(() => api.step("beat"))}
        onRun={() => {
          setPlaying(false);
          call(api.run);
        }}
        onPlay={() => setPlaying(!playing)}
        onSpeed={setSpeed}
      />
      {error && <div className="error">{error}</div>}
      {program && state && (
        <main>
          <section className="panel">
            <div className="panel-head">
              <h2>Datapath</h2>
              <div className="toggle">
                <button className={view === "detailed" ? "selected" : ""} onClick={() => chooseView("detailed")}>
                  Detailed
                </button>
                <button className={view === "blackbox" ? "selected" : ""} onClick={() => chooseView("blackbox")}>
                  Black box
                </button>
              </div>
            </div>
            <Datapath state={state} program={program} blackBox={view === "blackbox"} />
          </section>
          <div className="row">
            <section className="panel grow">
              <h2>Pipeline</h2>
              <PipelineTable state={state} />
            </section>
            <section className="panel side">
              <h2>Registers</h2>
              <RegistersPanel state={state} previous={previous} />
              <h2>Memory</h2>
              <MemoryPanel state={state} previous={previous} />
            </section>
          </div>
          <div className="row">
            <section className="panel half">
              <h2>Program</h2>
              <ProgramListing program={program} state={state} />
            </section>
            <section className="panel half">
              <h2>Microcode</h2>
              <MicrocodePanel program={program} state={state} />
            </section>
          </div>
          <section className="panel">
            <h2>Latches</h2>
            <LatchTable state={state} />
          </section>
        </main>
      )}
    </div>
  );
}
