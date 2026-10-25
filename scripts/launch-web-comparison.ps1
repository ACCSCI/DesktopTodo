param([ValidateRange(1, 20)][int]$Notes = 1, [ValidateRange(0, 1000)][int]$Tasks = 10)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$base = Join-Path $root '.tools/web-comparison'
$exe = Join-Path $base 'bin/DesktopTodo.WebComparison.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Run scripts/bootstrap-web-comparison.ps1 then npm run build:web first.' }
$data = Join-Path $base ('interactive-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $data | Out-Null
Start-Process -FilePath $exe -ArgumentList @(('"' + $data + '"'), ('"' + (Join-Path $base 'frontend') + '"'), $Notes, $Tasks) -WindowStyle Hidden
Write-Host "独立实验数据: $data"
