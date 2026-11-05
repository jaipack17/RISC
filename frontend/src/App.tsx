import { useCallback, useEffect, useRef, useState } from "react";
import * as api from "./api";
import type { CpuState, Processor, ProgramInfo } from "./types";
import Toolbar from "./components/Toolbar";
import Datapath from "./components/Datapath";
import FourStageDatapath from "./components/FourStageDatapath";
import PipelineTable from "./components/PipelineTable";
import ProgramListing from "./components/ProgramListing";
import MicrocodePanel from "./components/MicrocodePanel";
import RegistersPanel from "./components/RegistersPanel";
import MemoryPanel from "./components/MemoryPanel";
import LatchTable from "./components/LatchTable";
import StackPanel from "./components/StackPanel";
import ComparePage from "./components/compare/ComparePage";

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

  const [page, setPage] = useState<"simulator" | "compare">(() => {
    if (new URLSearchParams(window.location.search).get("page") === "compare") {
      return "compare";
    }
    try {
      return localStorage.getItem("page") === "compare" ? "compare" : "simulator";
    } catch {
      return "simulator";
    }
  });
  const [theme, setTheme] = useState<"dark" | "light">(() => {
    const asked = new URLSearchParams(window.location.search).get("theme"); // e.g. ?theme=light
    if (asked === "light" || asked === "dark") {
      return asked;
    }
    try {
      return localStorage.getItem("theme") === "light" ? "light" : "dark";
    } catch {
      return "dark";
    }
  });
  const [processor, setProcessor] = useState<Processor>(() => {
    const asked = new URLSearchParams(window.location.search).get("cpu"); // e.g. ?cpu=4
    if (asked === "4" || asked === "6") {
      return asked === "4" ? "four" : "six";
    }
    try {
      return localStorage.getItem("processor") === "four" ? "four" : "six";
    } catch {
      return "six";
    }
  });

  function choosePage(next: "simulator" | "compare") {
    setPlaying(false);
    setPage(next);
    try {
      localStorage.setItem("page", next);
    } catch {
      // the choice just isn't remembered
    }
  }

  function chooseView(next: "detailed" | "blackbox") {
    setView(next);
    try {
      localStorage.setItem("view", next);
    } catch {
      // the choice just isn't remembered
    }
  }

  function chooseProcessor(next: Processor) {
    setPlaying(false);
    setProcessor(next);
    try {
      localStorage.setItem("processor", next);
    } catch {
      // the choice just isn't remembered
    }
  }

  function chooseTheme(next: "dark" | "light") {
    setTheme(next);
    try {
      localStorage.setItem("theme", next);
    } catch {
      // the choice just isn't remembered
    }
  }

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
  }, [theme]);

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

  // On start and whenever the processor changes: its program and state (both processors hold the same program)
  useEffect(() => {
    Promise.all([api.getProgram(processor), api.getState(processor), api.listPrograms()])
      .then(([p, s, list]) => {
        setProgram(p);
        setPrevious(null);
        setState(s);
        setPrograms(list);
        setError("");
      })
      .catch(() => setError("Cannot reach the backend. Start it with ./build/risc_web from the project folder."));
  }, [processor]);

  // Play: one microcycle every `speed` ms until the program stops
  useEffect(() => {
    if (!playing) {
      return;
    }
    const timer = setInterval(() => call(() => api.step(processor, "microcycle")), speed);
    return () => clearInterval(timer);
  }, [playing, speed, call, processor]);

  useEffect(() => {
    if (state && state.status !== "running") {
      setPlaying(false);
    }
  }, [state]);

  async function load(name: string) {
    setPlaying(false);
    try {
      setProgram(await api.loadProgramFile(processor, name));
      const s = await api.getState(processor);
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
      setProgram(await api.loadProgramSource(processor, name, source));
      const s = await api.getState(processor);
      setPrevious(null);
      setState(s);
      setError("");
    } catch (e) {
      setError((e as Error).message);
    }
  }

  const pages = (
    <nav className="pages">
      <div className="toggle">
        <button className={page === "simulator" ? "selected" : ""} onClick={() => choosePage("simulator")}>
          Simulator
        </button>
        <button className={page === "compare" ? "selected" : ""} onClick={() => choosePage("compare")}>
          Compare 4 vs 6
        </button>
      </div>
      <button onClick={() => chooseTheme(theme === "dark" ? "light" : "dark")} title="Switch between the dark and the light theme">
        {theme === "dark" ? "Light" : "Dark"}
      </button>
    </nav>
  );

  if (page === "compare") {
    return (
      <div className="app">
        <header className="toolbar">
          <div className="toolbar-row">
            <span className="title">RISC201 · 4-stage vs 6-stage microprogrammed pipelines</span>
            <span className="spacer" />
            {pages}
          </div>
        </header>
        <ComparePage />
      </div>
    );
  }

  return (
    <div className="app">
      <Toolbar
        pages={pages}
        program={program}
        programs={programs}
        state={state}
        playing={playing}
        speed={speed}
        onLoad={load}
        onLoadSource={loadSource}
        onReset={() => {
          setPlaying(false);
          call(() => api.reset(processor));
        }}
        onStepMicrocycle={() => call(() => api.step(processor, "microcycle"))}
        onStepBeat={() => call(() => api.step(processor, "beat"))}
        onRun={() => {
          setPlaying(false);
          call(() => api.run(processor));
        }}
        onPlay={() => setPlaying(!playing)}
        onSpeed={setSpeed}
      />
      {error && <div className="error">{error}</div>}
      {program && state && program.processor === state.processor && (
        <main>
          <section className="panel">
            <div className="panel-head">
              <h2>Datapath</h2>
              <div className="toggles">
                <div className="toggle">
                  <button className={processor === "six" ? "selected" : ""} onClick={() => chooseProcessor("six")}>
                    6-stage
                  </button>
                  <button className={processor === "four" ? "selected" : ""} onClick={() => chooseProcessor("four")}>
                    4-stage
                  </button>
                </div>
                <div className="toggle">
                  <button className={view === "detailed" ? "selected" : ""} onClick={() => chooseView("detailed")}>
                    Detailed
                  </button>
                  <button className={view === "blackbox" ? "selected" : ""} onClick={() => chooseView("blackbox")}>
                    Black box
                  </button>
                </div>
              </div>
            </div>
            {state.processor === "four" ? (
              <FourStageDatapath state={state} program={program} blackBox={view === "blackbox"} />
            ) : (
              <Datapath state={state} program={program} blackBox={view === "blackbox"} />
            )}
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
            <section className="panel side-wide">
              <h2>Stack</h2>
              <StackPanel program={program} state={state} previous={previous} />
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
