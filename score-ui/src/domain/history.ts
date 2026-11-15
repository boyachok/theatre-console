import type { Project, Workspace } from './schema.ts';
export interface HistoryState {
  workspace: Workspace;
  past: Project[];
  future: Project[];
  at: number;
}
export type HistoryAction =
  | {
      type: 'set';
      update: Workspace | ((w: Workspace) => Workspace);
      at: number;
      discrete?: boolean;
    }
  | { type: 'undo' | 'redo'; at: number }
  | { type: 'clear' };
export function historyReducer(s: HistoryState, a: HistoryAction): HistoryState {
  if (a.type === 'clear') return { ...s, past: [], future: [], at: 0 };
  if (a.type === 'set') {
    const workspace = typeof a.update === 'function' ? a.update(s.workspace) : a.update;
    if (workspace.draft === s.workspace.draft) return { ...s, workspace };
    const past =
      !a.discrete && s.at > 0 && a.at - s.at < 350 && s.past.length
        ? s.past
        : [...s.past, s.workspace.draft].slice(-80);
    return { workspace, past, future: [], at: a.discrete ? 0 : a.at };
  }
  const source = a.type === 'undo' ? s.past : s.future;
  if (!source.length) return s;
  const target = source[source.length - 1];
  const draft = {
    ...target,
    revision: s.workspace.draft.revision + 1,
    updatedAt: new Date(a.at).toISOString(),
  };
  return {
    workspace: { ...s.workspace, draft },
    past: a.type === 'undo' ? s.past.slice(0, -1) : [...s.past, s.workspace.draft],
    future: a.type === 'undo' ? [...s.future, s.workspace.draft] : s.future.slice(0, -1),
    at: 0,
  };
}
