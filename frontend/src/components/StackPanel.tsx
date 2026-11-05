import type { CpuState, ProgramInfo } from "../types";
import { hex } from "../types";

// The full-descending stack (sp = r14), drawn top down like a frame diagram: the top of memory
// first, sp marked, the free words under it dimmed. A word saved from ra (the backend tracks
// which stores came from r15) starts a new frame, labelled with the function its call went to.

interface Frame {
  name: string;
  from: number; // highest address of the frame
}

// The call instruction a return address comes back from, e.g. "call fact"
function callBefore(program: ProgramInfo, value: number): string {
  const line = program.listing.find((l) => l.hasInstruction && l.address === value - 4);
  if (!line) {
    return "call";
  }
  return line.text.replace(/^\w+:\s*/, "");
}

export default function StackPanel({ program, state, previous }: { program: ProgramInfo; state: CpuState; previous: CpuState | null }) {
  const stack = state.stack;
  const sp = stack.sp;
  const used = stack.end - sp;
  const capacity = stack.end - stack.base;
  const words = [...stack.words].reverse(); // top of the stack first

  // What changed since the last update: a push moves sp down, a pop moves it up
  const before = new Map<number, number>();
  for (const w of previous?.stack.words ?? []) {
    before.set(w.address, w.value);
  }
  let motion = "";
  if (previous && previous.stack.sp !== sp && previous.processor === state.processor) {
    const delta = sp - previous.stack.sp;
    motion = delta < 0 ? `sp ${delta} (push)` : `sp +${delta} (pop)`;
  }

  // Frames, from the top: the words above the first saved return address belong to main
  const frames: Frame[] = [{ name: "main", from: stack.end - 4 }];
  const returnTo = new Map<number, string>();
  for (const w of words) {
    if (w.address < sp) {
      break;
    }
    if (w.ra) {
      const call = callBefore(program, w.value);
      returnTo.set(w.address, call);
      frames.push({ name: call.replace(/^call\s+/, ""), from: w.address });
    }
  }
  const frameOf = (address: number) => {
    let k = 0;
    for (let i = 0; i < frames.length; i++) {
      if (address <= frames[i].from) {
        k = i;
      }
    }
    return k;
  };

  return (
    <div className="stack">
      <div className="stack-head">
        <span>
          sp <b>{hex(sp)}</b>
        </span>
        <span>
          used <b>{used}</b> B · {used / 4} words
        </span>
        {motion && <span className="stack-motion">{motion}</span>}
      </div>
      <div className="stack-bar" title={`${used} of ${capacity} bytes (0x${stack.base.toString(16)} – 0x${stack.end.toString(16)})`}>
        <div style={{ width: `${Math.min(100, (100 * used) / capacity)}%` }} />
      </div>
      <div className="dim small stack-bounds">
        full at {hex(stack.base)} (StackOverflow below) · empty at {hex(stack.end, 5)} (StackUnderflow above)
      </div>
      {used === 0 && <p className="dim small">The stack is empty. push / pop, call and sub sp, sp, n use it.</p>}
      <div className="scroll">
        <table className="stack-table">
          <tbody>
            {sp === stack.end && (
              <tr className="top">
                <td className="sp-mark">sp →</td>
                <td className="dim">{hex(stack.end, 5)}</td>
                <td className="dim" colSpan={3}>
                  (empty stack)
                </td>
              </tr>
            )}
            {words.map((w) => {
              const free = w.address < sp;
              const k = frameOf(w.address);
              const frameTop = !free && frames[k].from === w.address;
              const changed = !free && before.has(w.address) && before.get(w.address) !== w.value;
              const pushedNow = !free && previous !== null && w.address < previous.stack.sp;
              let cls = free ? "free" : `frame-${k % 2}`;
              if (changed || pushedNow) {
                cls += " changed";
              }
              if (w.address === sp) {
                cls += " top";
              }
              const call = returnTo.get(w.address);
              return (
                <tr key={w.address} className={cls}>
                  <td className="sp-mark">{w.address === sp ? "sp →" : ""}</td>
                  <td className="dim">{hex(w.address)}</td>
                  <td className="mono">{free ? "" : hex(w.value, 8)}</td>
                  <td className="left note">{call ? `ra · after ${call}` : ""}</td>
                  <td className="left frame">{frameTop ? (k === 0 ? "main" : `${frames[k].name} #${k}`) : ""}</td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
}
