import { spawn } from 'node:child_process';
import { resolve } from 'node:path';
import { existsSync } from 'node:fs';

const sdk = resolve('.tools/native-0.11.0');
const cli = resolve('.tools/node_modules/@native-sdk/cli/bin/native.js');
if (!existsSync(resolve(sdk, 'build.zig')) || !existsSync(cli)) {
  throw new Error('Run scripts/bootstrap.ps1 before native checks.');
}
const child = spawn(process.execPath, [cli, ...process.argv.slice(2), '--yes'], {
  stdio: 'inherit', windowsHide: true,
  env: { ...process.env, NATIVE_SDK_PATH: sdk },
});
child.on('error', error => { console.error(error); process.exitCode = 1; });
child.on('exit', code => { process.exitCode = code ?? 1; });
