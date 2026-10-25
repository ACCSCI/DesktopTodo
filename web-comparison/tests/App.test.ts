import { render, screen, fireEvent, waitFor, cleanup } from '@testing-library/svelte';
import { afterEach, expect, test, vi } from 'vitest';
import App from '../src/App.svelte';
import type { Snapshot } from '../src/protocol';
afterEach(cleanup);
test('library controls call native domain bridge and lock stays in the header', async () => {
  const state: Snapshot = { revision: 0, note: { id: 1, title: '测试便签', color: 0xede9fe,
    mode: 2, locked: false, filter: 0, tasks: [], visible: [] } };
  window.nativeCommand = vi.fn(async (action, ...args) => {
    if (action === 'lock') state.note.locked = Boolean(args[0]);
    return structuredClone(state);
  });
  render(App);
  await screen.findByRole('heading', { name: '测试便签' });
  const lock = screen.getByRole('button', { name: '锁定' });
  const parent = lock.parentElement;
  await fireEvent.click(lock);
  await waitFor(() => expect(screen.getByRole('button', { name: '解锁' }).parentElement).toBe(parent));
  await fireEvent.click(screen.getByRole('button', { name: '解锁' }));
  await fireEvent.input(screen.getByRole('textbox', { name: '新待办' }), { target: { value: '中文输入' } });
  await fireEvent.submit(screen.getByRole('textbox', { name: '新待办' }).closest('form')!);
  expect(window.nativeCommand).toHaveBeenCalledWith('add', '中文输入');
});
