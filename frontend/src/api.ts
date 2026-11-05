import type { BenchmarkInfo, CompareResult, CpuState, Processor, ProgramInfo } from "./types";

// Calls to the C++ backend (backend/server.cpp). The six-stage processor answers unless processor is "four".

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

export function getProgram(processor: Processor): Promise<ProgramInfo> {
  return request("GET", "/program?processor=" + processor);
}

export function getState(processor: Processor): Promise<CpuState> {
  return request("GET", "/state?processor=" + processor);
}

export function listPrograms(): Promise<string[]> {
  return request("GET", "/programs");
}

// Both processors get the program
export function loadProgramFile(processor: Processor, program: string): Promise<ProgramInfo> {
  return request("POST", "/load?processor=" + processor, { program });
}

export function loadProgramSource(processor: Processor, name: string, source: string): Promise<ProgramInfo> {
  return request("POST", "/load?processor=" + processor, { name, source });
}

export function reset(processor: Processor): Promise<CpuState> {
  return request("POST", "/reset?processor=" + processor);
}

export function step(processor: Processor, unit: "microcycle" | "beat", count = 1): Promise<CpuState> {
  return request("POST", "/step?processor=" + processor, { unit, count });
}

export function run(processor: Processor): Promise<CpuState> {
  return request("POST", "/run?processor=" + processor);
}

export function listBenchmarks(): Promise<BenchmarkInfo[]> {
  return request("GET", "/benchmarks");
}

// A program from programs/ on fresh 4-stage, 6-stage and sequential processors, with the pipelines' timelines
export function compareProgram(program: string): Promise<CompareResult> {
  return request("POST", "/compare", { program });
}

// Every benchmark on the three processors (no timelines)
export function runBenchmark(): Promise<CompareResult[]> {
  return request("GET", "/benchmark");
}
