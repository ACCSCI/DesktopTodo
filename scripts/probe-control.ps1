param([Parameter(Mandatory=$true)][string]$Name, [string]$View = 'main-canvas')
$ErrorActionPreference = 'Stop'
$probeRoot = Split-Path $PSScriptRoot -Parent
$cli = Join-Path $probeRoot '.tools/node_modules/@native-sdk/cli/bin/native.js'
Push-Location (Join-Path $probeRoot 'window-probe')
try {
    $snapshot = (& node $cli automate snapshot) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Snapshot failed' }
    $pattern = 'widget @w\d+/' + [regex]::Escape($View) + '#(\d+) role=button name="' + [regex]::Escape($Name) + '"'
    $widgets = [regex]::Matches($snapshot, $pattern)
    if ($widgets.Count -ne 1) { throw "Expected one button: $View/$Name; found $($widgets.Count)" }
    & node $cli automate widget-click $View $widgets[0].Groups[1].Value
    if ($LASTEXITCODE -ne 0) { throw 'Dispatch failed' }
} finally { Pop-Location }
