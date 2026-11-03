import type { CpuState } from "../types";
import { hex } from "../types";

const NAMES = ["r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "sp", "ra"];

export default function RegistersPanel({ state, previous }: { state: CpuState; previous: CpuState | null }) {
  const alu = state.units.alu;
  return (
    <div>
      <table className="registers">
        <tbody>
          {state.registers.map((value, r) => {
            const changed = previous !== null && previous.registers[r] !== value;
            return (
              <tr key={r} className={changed ? "changed" : ""}>
                <td className="dim">{NAMES[r]}</td>
                <td className="mono">{hex(value, 8)}</td>
                <td className="mono dim">{value | 0}</td>
              </tr>
            );
          })}
        </tbody>
      </table>
      <p className="mono small">
        flags E={alu.flagE ? 1 : 0} GT={alu.flagGT ? 1 : 0}
      </p>
    </div>
  );
}
