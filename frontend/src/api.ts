import type { CpuState, ProgramInfo } from "./types";

// Calls to the C++ backend (backend/server.cpp)

async function request<T>(method: "GET" | "POST", path: string, body?: unknown): Promise<T> {
  const response = await fetch("/api" + path, {
    method,
    headers: body === undefined ? undefined : { "Content-Type": "application/json" },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  const data = await response.json();
  if (!response.ok) {
    throw new Error(data.error ?? `request failed (${response.status})`);
  }
  return data as T;
}

export function getProgram(): Promise<ProgramInfo> {
  return request("GET", "/program");
}

export function getState(): Promise<CpuState> {
  return request("GET", "/state");
}

export function listPrograms(): Promise<string[]> {
  return request("GET", "/programs");
}

export function loadProgramFile(program: string): Promise<ProgramInfo> {
  return request("POST", "/load", { program });
}

export function loadProgramSource(name: string, source: string): Promise<ProgramInfo> {
  return request("POST", "/load", { name, source });
}

export function reset(): Promise<CpuState> {
  return request("POST", "/reset");
}

export function step(unit: "microcycle" | "beat", count = 1): Promise<CpuState> {
  return request("POST", "/step", { unit, count });
}

export function run(): Promise<CpuState> {
  return request("POST", "/run");
}
