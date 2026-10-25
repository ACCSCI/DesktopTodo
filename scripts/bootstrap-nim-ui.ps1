$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$deps = Join-Path $root '.tools/nim-duilib'
New-Item $deps -ItemType Directory -Force | Out-Null
function Run($command, [string[]]$arguments) {
    & $command @arguments
    if ($LASTEXITCODE -ne 0) { throw "$command failed: $LASTEXITCODE" }
}
function Repo($url, $path, $revision) {
    if (!(Test-Path "$path/.git")) { Run git @('clone', '--no-checkout', $url, $path) }
    Run git @('-C', $path, 'fetch', '--depth', '1', 'origin', $revision)
    Run git @('-C', $path, 'checkout', '--detach', $revision)
}
$seven = (Get-Command 7z.exe -ErrorAction SilentlyContinue).Source
if (!$seven) { $seven = 'C:/Program Files/7-Zip/7z.exe' }
if (!(Test-Path $seven)) { throw 'Install 7-Zip first.' }
Repo 'https://github.com/rhett-lee/nim_duilib.git' "$deps/source" '9363b00e530cc57742e74834f3e81511ac98f222'
Repo 'https://github.com/rhett-lee/skia_compile.git' "$deps/skia_compile" 'd4b85325b979e68d02a1ab106c3e2d3bad821b28'
if (!(Test-Path "$deps/skia/BUILD.gn")) {
    Run node @('scripts/download-pinned.mjs', 'https://codeload.github.com/google/skia/zip/6f559bafbed4c8323a899df4008aa073df4eccc6', '86456386', 'bb3aeedf1c66a06a0e3e7e4c4da26dbd80ecaf8559955934c8c4ac29d739f47e', "$deps/skia.zip")
    # Expand-Archive avoids requiring Windows symlink privileges for this source zip.
    Expand-Archive "$deps/skia.zip" "$deps/skia-extract" -Force
    Move-Item -LiteralPath "$deps/skia-extract/skia-6f559bafbed4c8323a899df4008aa073df4eccc6" -Destination "$deps/skia"
}
if (!(Test-Path "$deps/llvm/bin/clang-cl.exe")) {
    Run node @('scripts/download-pinned.mjs', 'https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.4/LLVM-21.1.4-win64.exe', '374126146', '8e39f8051cd10a163c1fab304f16d8e16a9c0f5e94b59d08853bbab630bd65dd', "$deps/llvm.exe")
    Run $seven @('x', "$deps/llvm.exe", "-o$deps/llvm", '-y')
}
Run $seven @('x', "$deps/skia_compile/skia.2026-09-16.src.zip", "-o$deps/skia", '-y')
New-Item "$deps/skia/bin" -ItemType Directory -Force | Out-Null
Copy-Item "$deps/skia_compile/windows/bin/gn.exe", "$deps/skia_compile/windows/bin/ninja.exe" "$deps/skia/bin" -Force
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio 2022 C++ Build Tools and Windows SDK are required.' }
$toolchain = (Get-Content "$vs/VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt").Trim()
$sdk = (Get-ItemProperty 'HKLM:/SOFTWARE/Microsoft/Windows Kits/Installed Roots').KitsRoot10
$sdkVersion = (Get-ChildItem "$sdk/Include" -Directory | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1).Name
$build = "$deps/skia/out/llvm.x64.release"
New-Item $build -ItemType Directory -Force | Out-Null
$template = Get-Content "$root/nim-ui/skia-args.gn.in" -Raw
$template.Replace('@LLVM@', "$deps/llvm".Replace('\','/')).Replace('@VC@', "$vs/VC".Replace('\','/')).Replace('@SDK@', $sdk.Replace('\','/').TrimEnd('/')).Replace('@SDK_VERSION@', $sdkVersion).Replace('@VC_VERSION@', $toolchain) | Set-Content "$build/args.gn" -Encoding utf8
$python = (Get-Command python.exe).Source
Run "$deps/skia/bin/gn.exe" @('gen', $build, "--root=$deps/skia", "--script-executable=$python")
Run "$deps/skia/bin/ninja.exe" @('-C', $build, '-j8', 'skia', 'svg', 'skshaper', 'skottie', 'sksg', 'jsonreader')
Write-Host 'Dependencies ready. Run scripts/build-nim-ui.ps1.'

