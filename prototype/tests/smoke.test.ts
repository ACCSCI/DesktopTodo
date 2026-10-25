import assert from 'node:assert/strict';
import test from 'node:test';
import { NativeApp, findWidget } from '@native-sdk/core/testing';

test('compiled headless app dispatches actual button input', async () => {
  const app = await NativeApp.start({ wallMs: 77000 });
  try {
    const initial = await app.snapshot();
    const changed = await app.click(findWidget(initial, { role: 'button', name: '+' }));
    assert.equal(changed.model.count, 1);
    const replay = await app.verifyReplay();
    assert.deepEqual(replay.snapshot.model, changed.model);
  } finally {
    await app.close();
  }
});
