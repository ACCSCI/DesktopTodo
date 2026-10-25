export interface Task { id: number; text: string; icon: string; color: number; status: number; }
export interface Note { id: number; title: string; color: number; mode: number; locked: boolean; filter: number; tasks: Task[]; visible: number[]; }
export interface Snapshot { note: Note; revision: number; error?: string; }
declare global {
  interface Window {
    nativeCommand: (action: string, ...args: unknown[]) => Promise<Snapshot>;
    receiveNote: (state: Snapshot) => void;
    comparisonReady?: boolean;
  }
}
export function hex(color: number): string { return `#${color.toString(16).padStart(6, '0')}`; }
export function visibleTasks(note: Note): Task[] {
  const ids = new Set(note.visible);
  return note.tasks.filter(task => ids.has(task.id));
}
export function request(action: string, ...args: unknown[]): Promise<Snapshot> {
  if (!window.nativeCommand) return Promise.reject(new Error('请在 WebView2 对比宿主中运行。'));
  return window.nativeCommand(action, ...args);
}
