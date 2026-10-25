$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$cliPath = Join-Path $projectRoot '.tools/node_modules/@native-sdk/cli/bin/native.js'
$probeDirectory = Join-Path $projectRoot 'window-probe'
$probeExe = Join-Path $probeDirectory 'zig-out/bin/window-probe.exe'
$imageDirectory = Join-Path $projectRoot 'docs/images'
New-Item -ItemType Directory -Force $imageDirectory | Out-Null
Set-Location $probeDirectory

function Invoke-ProbeCommand {
    $output = & node $cliPath automate @args
    if ($LASTEXITCODE -ne 0) { throw "Automation failed: $args" }
    return ($output -join "`n")
}
function Click-ProbeWidget([string]$view, [string]$name) {
    $snapshot = Invoke-ProbeCommand snapshot
    $pattern = 'widget @w\d+/' + [regex]::Escape($view) + '#(\d+) role=button name="' + [regex]::Escape($name) + '"'
    $widget = [regex]::Match($snapshot, $pattern)
    if (!$widget.Success) { throw "Missing button $view/$name" }
    Invoke-ProbeCommand widget-click $view $widget.Groups[1].Value | Out-Null
}

$probeProcess = Start-Process -FilePath $probeExe -WorkingDirectory $probeDirectory -WindowStyle Hidden `
    -RedirectStandardOutput 'probe.stdout.log' -RedirectStandardError 'probe.stderr.log' -PassThru
try {
    Invoke-ProbeCommand assert "publisher_pid=$($probeProcess.Id)" 'gpu_nonblank=true' 'role=button name="Overlay probe"' | Out-Null
    Click-ProbeWidget 'main-canvas' '+'
    Invoke-ProbeCommand assert 'total: 1' | Out-Null
    Click-ProbeWidget 'main-canvas' 'Overlay probe'
    Invoke-ProbeCommand assert 'Todo Overlay Probe' 'Todo Unlock Probe' 'Locked body' 'role=button name="Unlock"' | Out-Null
    foreach ($view in @('main-canvas', 'note-canvas', 'unlock-canvas')) {
        Invoke-ProbeCommand screenshot $view | Out-Null
        Copy-Item -LiteralPath ".zig-cache/native-sdk-automation/screenshot-$view.png" `
            -Destination (Join-Path $imageDirectory "$view.png") -Force
    }
    Click-ProbeWidget 'unlock-canvas' 'Unlock'
    Invoke-ProbeCommand assert 'Unlocked body' 'role=button name="Lock"' | Out-Null
    Click-ProbeWidget 'note-canvas' 'Increment from note'
    Invoke-ProbeCommand assert 'total: 2' 'Count: 2' | Out-Null
    Click-ProbeWidget 'unlock-canvas' 'Lock'
    Invoke-ProbeCommand assert 'Locked body' 'role=button name="Unlock"' | Out-Null
    # Close auxiliary windows before measuring the idle main window.
    Click-ProbeWidget 'main-canvas' 'Overlay probe'
    # SDK snapshots retain closed windows in their window table. Views and
    # widgets disappear on close; assert those live objects instead of titles.
    Invoke-ProbeCommand assert --absent 'view @w\d+/note-canvas' 'view @w\d+/unlock-canvas' | Out-Null
    $baseline = (Get-Process -Id $probeProcess.Id).TotalProcessorTime.TotalSeconds
    $watch = [Diagnostics.Stopwatch]::StartNew()
    Start-Sleep -Seconds 10
    $sample = Get-Process -Id $probeProcess.Id
    $measurement = [ordered]@{
        Context = 'ReleaseFast Zig scaffold, automation enabled, one window, 10 second sample'
        ElapsedSeconds = $watch.Elapsed.TotalSeconds
        CpuSeconds = $sample.TotalProcessorTime.TotalSeconds - $baseline
        WorkingSetMiB = [math]::Round($sample.WorkingSet64 / 1MB, 2)
        PrivateMiB = [math]::Round($sample.PrivateMemorySize64 / 1MB, 2)
        Handles = $sample.HandleCount
    }
    $measurement | ConvertTo-Json | Set-Content (Join-Path $projectRoot 'docs/probe-idle-measurement.json')
    $measurement | ConvertTo-Json
    Write-Output 'Windows SDK automation smoke passed; OS hit testing and desktop attachment remain separate checks.'
} finally {
    $running = Get-Process -Id $probeProcess.Id -ErrorAction SilentlyContinue
    if ($running -and $running.Path -eq $probeExe) { Stop-Process -Id $probeProcess.Id }
    Set-Location $projectRoot
}
