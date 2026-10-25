// Reusable ranged download; only the complete pinned SHA-256 is accepted.
import { spawn } from 'node:child_process';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve, dirname, join } from 'node:path';
const [url, size, expected, name] = process.argv.slice(2);
const bytes = Number(size);
if (!url?.startsWith('https://') || !Number.isSafeInteger(bytes) || bytes <= 0 || !/^[a-f0-9]{64}$/.test(expected ?? '') || !name)
  throw new Error('Usage: download-pinned.mjs HTTPS_URL BYTES SHA256 OUTPUT');
const output = resolve(name), directory = `${output}.parts`;
await mkdir(directory, { recursive: true });
const width = Math.ceil(bytes / 8);
const chunks = await Promise.all(Array.from({length:8},async (_,index) => {
  const start=index*width, end=Math.min(bytes,start+width)-1;
  const file=join(directory,`${index}.part`);
  let chunk;
  try { chunk=await readFile(file); } catch {}
  if(chunk?.length!==end-start+1) {
    await new Promise((ok,fail) => {
      const child=spawn('curl.exe',['-fsSL','--retry','4','--retry-all-errors','--connect-timeout','15','--max-time','600',
        '--range',`${start}-${end}`,url,'-o',file],{windowsHide:true,stdio:'inherit'});
      child.on('error',fail);child.on('exit',code=>code===0?ok():fail(new Error(`part ${index}: ${code}`)));
    });
    chunk=await readFile(file);
  }
  if(chunk.length!==end-start+1) throw new Error(`Invalid range: ${index}`);
  console.log(`Received ${index+1}/8`);
  return chunk;
}));
const archive=Buffer.concat(chunks);
if(createHash('sha256').update(archive).digest('hex')!==expected) throw new Error('SHA-256 mismatch');
await mkdir(dirname(output),{recursive:true});await writeFile(output,archive);
console.log(`Verified ${output}`);
