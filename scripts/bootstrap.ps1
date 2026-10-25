$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$sdkArchive = '.tools/native-v0.11.0-complete.tar.gz'
$sourceHash = '627924F6EF218039458714EA3322F6F61088C26A76B9799888F7C49D57FECE82'
New-Item -ItemType Directory -Force .tools | Out-Null
npm install --ignore-scripts --no-save --prefix .tools '@native-sdk/cli@0.11.0'
if ($LASTEXITCODE -ne 0) { throw 'CLI install failed' }
if (!(Test-Path $sdkArchive) -or (Get-FileHash $sdkArchive).Hash -ne $sourceHash) {
    curl.exe -fL --retry 3 --retry-all-errors --connect-timeout 15 --max-time 600 `
        'https://codeload.github.com/vercel-labs/native/tar.gz/refs/tags/v0.11.0' -o $sdkArchive
    if ($LASTEXITCODE -ne 0) { throw 'Source download failed' }
}
if ((Get-FileHash $sdkArchive).Hash -ne $sourceHash) { throw 'Pinned source SHA-256 mismatch' }
tar -xzf $sdkArchive -C .tools
if ($LASTEXITCODE -ne 0) { throw 'Source extraction failed' }
node scripts/patch-sdk.mjs
if ($LASTEXITCODE -ne 0) { throw 'SDK compatibility patch failed' }
npm install --ignore-scripts --prefix .tools/native-0.11.0/packages/core --include=dev
if ($LASTEXITCODE -ne 0) { throw 'Frontend dependencies failed' }
node scripts/download-compiler.mjs
if ($LASTEXITCODE -ne 0) { throw 'Compiler download failed' }
node scripts/install-toolchain.mjs
if ($LASTEXITCODE -ne 0) { throw 'Toolchain install failed' }
if (Get-Command lefthook -ErrorAction SilentlyContinue) {
    lefthook install
} else {
    npx --yes lefthook@2.1.6 install
}
if ($LASTEXITCODE -ne 0) { throw 'Lefthook install failed' }
