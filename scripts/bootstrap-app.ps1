$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
if ((& zig version) -ne '0.16.0') { throw 'Install Zig 0.16.0 before building DesktopTodo.' }
$archive = '.tools/sqlite-amalgamation-3530400.zip'
$digest = '1E71DDF93849C6A6ECF58B827C0692073D2DD7EE40196158068F7B29F422E87D'
New-Item -ItemType Directory -Force .tools/sqlite | Out-Null
if (!(Test-Path $archive) -or (Get-FileHash $archive).Hash -ne $digest) {
    curl.exe -fL --retry 3 --retry-all-errors --connect-timeout 15 --max-time 180 `
        'https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip' -o $archive
    if ($LASTEXITCODE -ne 0) { throw 'SQLite download failed' }
}
if ((Get-FileHash $archive).Hash -ne $digest) { throw 'SQLite archive digest mismatch' }
Expand-Archive -LiteralPath $archive -DestinationPath .tools/sqlite-upstream -Force
Copy-Item .tools/sqlite-upstream/sqlite-amalgamation-3530400/sqlite3.c .tools/sqlite/sqlite3.c -Force
Copy-Item .tools/sqlite-upstream/sqlite-amalgamation-3530400/sqlite3.h .tools/sqlite/sqlite3.h -Force
if (Get-Command lefthook -ErrorAction SilentlyContinue) { lefthook install }
else { npx --yes lefthook@2.1.6 install }
if ($LASTEXITCODE -ne 0) { throw 'Lefthook install failed' }
Write-Output 'DesktopTodo app dependencies ready; Native SDK experiments are optional.'
