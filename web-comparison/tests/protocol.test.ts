import { describe, expect, it } from 'vitest';
import { hex, visibleTasks, request } from '../src/protocol';
describe('native protocol adapter', () => {
  it('preserves native filtered order and Unicode without recreating domain status rules', () => {
    const tasks = [1, 2, 3].map(id => ({ id, text: '中文 🎯', icon: '•', color: 0, status: 0 }));
    expect(visibleTasks({ id: 1, title: '', color: 0, mode: 2, locked: false, filter: 1, tasks, visible: [1, 3] }).map(t => t.id)).toEqual([1, 3]);
    expect(hex(0x123)).toBe('#000123');
  });
  it('fails honestly when there is no native bridge', async () => {
    await expect(request('snapshot')).rejects.toThrow('WebView2');
  });
});
