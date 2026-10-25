// Preserve upstream installer version/ABI/digest checks; use curl for slow
// GitHub transfers, where Node's two-minute asset timeout is insufficient.
import { spawn } from 'node:child_process';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve, join, basename } from 'node:path';
import { pathToFileURL } from 'node:url';

const sdk = resolve('.tools/native-0.11.0');
const cache = resolve('.tools/toolchain-downloads');
await mkdir(cache, { recursive: true });
const manifest = JSON.parse(await readFile(join(sdk, 'packages/core/scripts/scriptc-toolchain.json'), 'utf8'));
function curl(args) {
  return new Promise((accept, reject) => {
    const child = spawn('curl.exe', ['-fsSL', '--retry', '3', '--retry-all-errors', '--connect-timeout', '15', '--max-time', '600', ...args],
      { windowsHide: true, stdio: 'inherit' });
    child.on('error', reject);
    child.on('exit', code => code === 0 ? accept() : reject(new Error(`curl exited ${code}`)));
  });
}
async function acquire(asset) {
  const output = join(cache, basename(new URL(asset.url).pathname));
  const verify = data => data.length === asset.bytes
    && createHash('sha256').update(data).digest('hex') === asset.sha256;
  try { if (verify(await readFile(output))) return output; } catch {}
  if (asset === manifest.releases['win32-x64-msvc']) {
    const host = await readFile(resolve('.tools/scriptc-verified.tar.gz'));
    if (!verify(host)) throw new Error('Host release digest mismatch');
    await writeFile(output, host);
    return output;
  }
  const parts = await Promise.all(Array.from({ length: 8 }, async (_, index) => {
    const width = Math.ceil(asset.bytes / 8);
    const start = index * width;
    const end = Math.min(asset.bytes, start + width) - 1;
    const file = `${output}.${index}.part`;
    let data;
    try { data = await readFile(file); } catch {}
    if (data?.length !== end - start + 1) {
      await curl(['--range', `${start}-${end}`, asset.url, '-o', file]);
      data = await readFile(file);
    }
    if (data.length !== end - start + 1) throw new Error(`Invalid range from ${asset.url}`);
    return data;
  }));
  const data = Buffer.concat(parts);
  if (!verify(data)) throw new Error(`Digest mismatch: ${asset.url}`);
  await writeFile(output, data);
  console.log(`SHA-256 verified: ${basename(output)}`);
  return output;
}
const entries = Object.values(manifest.releases);
const packages = Object.values(manifest.packages);
async function acquirePackage(asset) {
  const output = join(cache, basename(new URL(asset.url).pathname));
  const verify = data => `sha512-${createHash('sha512').update(data).digest('base64')}` === asset.integrity;
  try { if (verify(await readFile(output))) return output; } catch {}
  // Transport mirror only; package bytes must match upstream's SHA-512.
  const transport = asset.url.replace('https://registry.npmjs.org/', 'https://registry.npmmirror.com/');
  await curl([transport, '-o', output]);
  if (!verify(await readFile(output))) throw new Error(`Package integrity mismatch: ${asset.name}`);
  console.log(`SHA-512 verified: ${asset.name}`);
  return output;
}
// Two releases at a time; each release uses eight ranges.
for (let index = 0; index < entries.length; index += 2) {
  await Promise.all(entries.slice(index, index + 2).map(acquire));
}
const { installScriptc } = await import(pathToFileURL(join(sdk, 'packages/core/scripts/install_scriptc.mjs')));
const result = await installScriptc(pathToFileURL(join(sdk, 'packages/core/package.json')), {
  fetch: async (url, options) => {
    const asset = entries.find(entry => entry.url === url);
    if (asset) return new Response(await readFile(await acquire(asset)));
    const pkg = packages.find(entry => entry.url === url);
    if (pkg) return new Response(await readFile(await acquirePackage(pkg)));
    throw new Error(`Unexpected upstream URL: ${url}`);
  },
});
console.log(`Upstream installer verified complete toolchain: ${result.root}`);
