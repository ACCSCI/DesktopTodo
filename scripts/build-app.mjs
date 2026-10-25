import { spawn } from 'node:child_process';
import { mkdir, readFile, writeFile, readdir, stat } from 'node:fs/promises';
import { resolve, join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const kind = process.argv[2] ?? 'test';
if (!['test', 'release', 'debug'].includes(kind)) throw new Error(`Unknown build: ${kind}`);
const build = join(root, '.tools/app-build');
const sqlite = join(root, '.tools/sqlite');
await mkdir(build, { recursive: true });
const pins = {
  'sqlite3.c': 'b1dd5d74ec7f29055a6684fa06fb3c2f6821c87dd38f9a458dfd2e8a1db28189',
  'sqlite3.h': '919e7f2e8ed1d8f56ac17b412b8971c76aa5d1a879752cc6058f75e7d5910e1d',
};
for (const [name, digest] of Object.entries(pins)) {
  const bytes = await readFile(join(sqlite, name)).catch(() => {
    throw new Error('SQLite sources missing. Run scripts/bootstrap-app.ps1.');
  });
  if (createHash('sha256').update(bytes).digest('hex') !== digest) throw new Error(`SQLite source digest mismatch: ${name}`);
}
function run(command, args) {
  return new Promise((accept, reject) => {
    const child = spawn(command, args, { cwd: root, windowsHide: true, stdio: 'inherit' });
    child.on('error', reject);
    child.on('exit', code => code === 0 ? accept() : reject(new Error(`${command} failed: ${code}`)));
  });
}
async function files(directory, suffix) {
  const found = [];
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const file = join(directory, entry.name);
    if (entry.isDirectory()) found.push(...await files(file, suffix));
    else if (file.endsWith(suffix)) found.push(file);
  }
  return found.sort();
}
async function current(output, hash) {
  try { await stat(output); return (await readFile(`${output}.hash`, 'utf8')) === hash; } catch { return false; }
}
const sqliteObject = join(build, 'sqlite3.o');
const resource = join(build, 'desktop-todo.res');
const resourceHash = createHash('sha256')
  .update(await readFile(join(root,'app/resources/app.rc')))
  .update(await readFile(join(root,'app/resources/app.ico')))
  .update(await readFile(join(root,'app/resources/windows.manifest'))).digest('hex');
if (!await current(resource, resourceHash)) {
  await run('zig', ['rc', '/I', join(root,'app/resources'), '/fo', resource, join(root,'app/resources/app.rc')]);
  await writeFile(`${resource}.hash`, resourceHash);
}
const sqliteFlags = ['cc', '-std=c11', '-Os', '-DNDEBUG', '-DSQLITE_THREADSAFE=1',
  '-DSQLITE_OMIT_LOAD_EXTENSION', '-DSQLITE_DQS=0', '-c', join(sqlite, 'sqlite3.c'), '-o', sqliteObject];
const sqliteHash = createHash('sha256').update(JSON.stringify(sqliteFlags) + pins['sqlite3.c']).digest('hex');
if (!await current(sqliteObject, sqliteHash)) {
  console.log('Compiling pinned SQLite 3.53.4…');
  await run('zig', sqliteFlags);
  await writeFile(`${sqliteObject}.hash`, sqliteHash);
}
const core = [...await files(join(root, 'app/src/core'), '.cpp'), ...await files(join(root, 'app/src/storage'), '.cpp')];
const sources = kind === 'test' ? [...core, ...await files(join(root, 'app/src/platform'), '.cpp'), ...await files(join(root, 'tests/native'), '.cpp')] :
  [...core, ...await files(join(root, 'app/src/platform'), '.cpp'), ...await files(join(root, 'app/src/ui'), '.cpp'), join(root, 'app/src/main.cpp')];
const output = kind === 'test' ? join(build, 'todo-tests.exe') :
  process.argv[3] ? resolve(root,process.argv[3]) : join(root, 'dist', kind === 'debug' ? 'DesktopTodo-debug.exe' : 'DesktopTodo.exe');
await mkdir(dirname(output), { recursive: true });
const flags = ['c++', '-std=c++20', kind === 'release' ? '-Os' : '-O0', kind === 'release' ? '-s' : '-g',
  '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
  '-Wall', '-Wextra', '-Wpedantic', '-Wno-nullability-completeness', '-Wno-unused-command-line-argument',
  '-I', join(root, 'app/include'), '-I', sqlite,
  ...sources, sqliteObject, resource, '-o', output];
flags.push('-luser32', '-lgdi32', '-lcomctl32', '-lshell32', '-lole32', '-ladvapi32',
  '-lcomdlg32', '-lshlwapi', '-ldwmapi', '-limm32');
if (kind !== 'test') flags.push('-municode', '-Wl,--subsystem,windows');
const hash = createHash('sha256').update(JSON.stringify(flags) + sqliteHash + resourceHash);
for (const file of [...sources, ...await files(join(root, 'app/include'), '.hpp'), ...await files(join(root, 'tests/native'), '.hpp')]) {
  hash.update(file).update(await readFile(file));
}
const fingerprint = hash.digest('hex');
if (!await current(output, fingerprint)) {
  console.log(`Compiling DesktopTodo ${kind} (${sources.length} source files)…`);
  await run('zig', flags);
  await writeFile(`${output}.hash`, fingerprint);
}
if (kind === 'test') await run(output, []);
else console.log(`Built ${output} (${(await stat(output)).size} bytes)`);
