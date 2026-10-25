// Parallel ranged transfer; upstream SHA-256 remains the acceptance criterion.
import { spawn } from 'node:child_process';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve, join } from 'node:path';

const url = 'https://github.com/vercel-labs/scriptc/releases/download/v0.3.0/scriptc-0.3.0-win32-x64-msvc.tar.gz';
const bytes = 70137460;
const expected = 'f7b436755f33900bc97334f9cf701d4f5895f561ecc1b7d604541abcefaa5aac';
const directory = resolve('.tools/compiler-parts');
await mkdir(directory, { recursive: true });
const count = 8;
const width = Math.ceil(bytes / count);
const chunks = await Promise.all(Array.from({ length: count }, async (_, index) => {
  const start = index * width;
  const end = Math.min(bytes, start + width) - 1;
  const file = join(directory, `${index}.part`);
  let chunk;
  try { chunk = await readFile(file); } catch {}
  if (chunk?.length !== end - start + 1) {
    await new Promise((accept, reject) => {
      const child = spawn('curl.exe', ['-fsSL', '--retry', '2', '--connect-timeout', '15',
        '--max-time', '600', '--range', `${start}-${end}`, url, '-o', file],
      { windowsHide: true, stdio: 'inherit' });
      child.on('error', reject);
      child.on('exit', code => code === 0 ? accept() : reject(new Error(`curl part ${index}: ${code}`)));
    });
    chunk = await readFile(file);
  }
  if (chunk.length !== end - start + 1) throw new Error(`Invalid range length: ${index}`);
  console.log(`Verified length of part ${index + 1}/${count}`);
  return chunk;
}));
const archive = Buffer.concat(chunks);
if (createHash('sha256').update(archive).digest('hex') !== expected) throw new Error('SHA-256 mismatch');
await writeFile(resolve('.tools/scriptc-verified.tar.gz'), archive);
console.log('Compiler archive matches upstream SHA-256.');
