import assert from 'node:assert/strict';
import { mkdtemp, writeFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import test from 'node:test';

test('line gate allows 599 lines and rejects 600, including trailing newline', async () => {
  const fixture = await mkdtemp(join(tmpdir(), 'desktop-todo-gate-'));
  const script = resolve('scripts/check-lines.mjs');
  const run = () => spawnSync(process.execPath, [script, fixture], { encoding: 'utf8' });
  try {
    await writeFile(join(fixture, 'example.ts'), 'x\n'.repeat(599));
    assert.equal(run().status, 0);
    await writeFile(join(fixture, 'example.ts'), 'x\n'.repeat(600));
    assert.equal(run().status, 1);
    await writeFile(join(fixture, 'example.ts'), Array(600).fill('x').join('\n'));
    assert.equal(run().status, 1);
    await writeFile(join(fixture, 'example.ts'), '');
    await writeFile(join(fixture, '.config'), 'x\n'.repeat(600));
    assert.equal(run().status, 1);
  } finally {
    await rm(fixture, { recursive: true, force: true });
  }
});
