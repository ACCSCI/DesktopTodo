// Native SDK 0.11.0: Zig/COFF cannot combine linked native archives in
// build-obj. Keep semantic analysis intact using a static library on Windows.
import { readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';

const file = resolve('.tools/native-0.11.0/build/app.zig');
const original = '    const analysis_obj = b.addObject(.{\n';
const patched = `    const analysis_obj = if (target.result.os.tag == .windows) b.addLibrary(.{
        .name = b.fmt("{s}-analysis", .{app_options.name}),
        .root_module = analysis_mod,
        .linkage = .static,
        .use_llvm = useLlvmWorkaround(target),
    }) else b.addObject(.{
`;
const source = await readFile(file, 'utf8');
if (source.includes(patched)) {
  console.log('Windows semantic-analysis patch already applied.');
} else {
  if (source.split(original).length !== 2) throw new Error('Unexpected SDK source; refusing ambiguous patch');
  await writeFile(file, source.replace(original, patched));
  console.log('Applied Windows semantic-analysis patch; no test steps removed.');
}
