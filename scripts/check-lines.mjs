import { readdir, readFile } from 'node:fs/promises';
import { extname, join, relative, resolve } from 'node:path';

const excluded = new Set(['.git', '.tools', '.native', '__pycache__', 'node_modules', '.zig-cache', 'zig-out', 'dist']);
const root = resolve(process.argv[2] ?? '.');
const failures = [];
const binaryExtensions = new Set(['.png', '.ico', '.jpg', '.jpeg', '.gif', '.webp', '.woff', '.woff2', '.ttf', '.otf', '.pdf']);
const lockFiles = new Set(['package-lock.json', 'npm-shrinkwrap.json', 'pnpm-lock.yaml', 'yarn.lock', 'bun.lockb']);
async function walk(dir) {
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    if (excluded.has(entry.name)) continue;
    const path = join(dir, entry.name);
    if (entry.isDirectory()) await walk(path);
    else if (entry.isFile() && !binaryExtensions.has(extname(entry.name).toLowerCase())
      && !lockFiles.has(entry.name) && !entry.name.endsWith('.log') && entry.name !== '.probe-pid') {
      const source = await readFile(path, 'utf8');
      const lines = source === '' ? 0 : source.split('\n').length - Number(source.endsWith('\n'));
      if (lines >= 600) failures.push(`${relative(root, path)}: ${lines} lines (maximum 599)`);
    }
  }
}
await walk(root);
if (failures.length) {
  console.error(failures.join('\n'));
  process.exitCode = 1;
} else console.log('Line gate passed: all project text files < 600 lines.');
