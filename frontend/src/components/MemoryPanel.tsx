import type { CpuState } from "../types";
import { hex } from "../types";

// The data and stack words that are not 0
export default function MemoryPanel({ state, previous }: { state: CpuState; previous: CpuState | null }) {
  if (state.memory.length === 0) {
    return <p className="dim small">All data and stack words are 0.</p>;
  }
  const before = new Map<number, number>();
  for (const m of previous?.memory ?? []) {
    before.set(m.address, m.value);
  }
  return (
    <table className="registers">
      <tbody>
        {state.memory.map((m) => (
          <tr key={m.address} className={previous !== null && before.get(m.address) !== m.value ? "changed" : ""}>
            <td className="dim">{hex(m.address)}</td>
            <td className="mono">{hex(m.value, 8)}</td>
            <td className="mono dim">{m.value | 0}</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}
