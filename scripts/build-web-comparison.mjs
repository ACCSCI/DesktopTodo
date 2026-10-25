import { spawn } from 'node:child_process';
import { readdir, mkdir, copyFile } from 'node:fs/promises';
import { resolve, join } from 'node:path';
const root = resolve(import.meta.dirname, '..');
const deps = join(root, '.tools/web-comparison');
async function run(command, args) {
  await new Promise((ok, fail) => {
    const child = spawn(command, args, { cwd: root, stdio: 'inherit', windowsHide: true });
    child.on('error', fail);
    child.on('exit', code => code === 0 ? ok() : fail(new Error(`${command}: ${code}`)));
  });
}
await run(process.execPath, [join(root, 'web-comparison/node_modules/vite/bin/vite.js'), 'build', join(root, 'web-comparison')]);
const core = [];
for (const folder of ['core', 'storage']) {
  for (const file of await readdir(join(root, 'app/src', folder)))
    if (file.endsWith('.cpp')) core.push(join(root, 'app/src', folder, file));
}
await mkdir(join(deps, 'bin'), { recursive: true });
await copyFile(join(deps, 'sdk/build/native/x64/WebView2Loader.dll'), join(deps, 'bin/WebView2Loader.dll'));
await run('zig', ['c++', '-std=c++20', '-Os', '-s', '-DUNICODE', '-D_UNICODE', '-DNOMINMAX',
  '-DWEBVIEW_STATIC', '-DWEBVIEW_MSWEBVIEW2_BUILTIN_IMPL=0', '-DWEBVIEW_MSWEBVIEW2_EXPLICIT_LINK=1',
  '-I', join(deps, 'webview-0.12.0/core/include'),
  '-I', join(deps, 'webview-0.12.0/compatibility/mingw/include'),
  '-I', join(deps, 'sdk/build/native/include'), '-I', deps,
  '-I', join(root, 'app/include'), '-I', join(root, '.tools/sqlite'),
  join(root, 'web-comparison/native/main.cpp'), ...core,
  join(root, '.tools/app-build/sqlite3.o'), '-o', join(deps, 'bin/DesktopTodo.WebComparison.exe'),
  '-luser32', '-lole32', '-loleaut32', '-lshell32', '-lshlwapi', '-lversion', '-ladvapi32', '-lgdi32', '-luuid']);
