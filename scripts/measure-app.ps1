$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$directory = Join-Path $root ('.tools/performance-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$exe = Join-Path $root 'dist/DesktopTodo.exe'
$appProcess = Start-Process -FilePath $exe -ArgumentList @('--automation', '--startup', '--data-dir', ('"' + $directory + '"')) -WindowStyle Hidden -PassThru
$samples = @()
try {
    & python "$PSScriptRoot/app_client.py" --data-dir $directory snapshot | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'App did not start' }
    foreach ($load in @(@(1, 10), @(10, 100))) {
        & python "$PSScriptRoot/app_client.py" --data-dir $directory seed $load[0] $load[1] | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Seed failed' }
        Start-Sleep -Seconds 2
        $before = Get-Process -Id $appProcess.Id
        $cpuBefore = $before.TotalProcessorTime.TotalSeconds
        $clock = [Diagnostics.Stopwatch]::StartNew()
        Start-Sleep -Seconds 10
        $after = Get-Process -Id $appProcess.Id
        $elapsed = $clock.Elapsed.TotalSeconds
        $samples += [ordered]@{
            notes = $load[0]; tasksPerNote = $load[1]
            seconds = [math]::Round($elapsed, 3)
            cpuSeconds = [math]::Round($after.TotalProcessorTime.TotalSeconds - $cpuBefore, 6)
            oneCoreCpuPercent = [math]::Round(100 * ($after.TotalProcessorTime.TotalSeconds - $cpuBefore) / $elapsed, 3)
            workingSetMiB = [math]::Round($after.WorkingSet64 / 1MB, 2)
            privateMiB = [math]::Round($after.PrivateMemorySize64 / 1MB, 2)
            handles = $after.HandleCount
        }
    }
    $result = [ordered]@{
        measuredAt = [DateTimeOffset]::Now.ToString('o')
        executableBytes = (Get-Item $exe).Length
        note = 'Local Windows measurement; desktop locked; automation enabled, no snapshot polling during idle sample.'
        samples = $samples
    }
    $result | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 (Join-Path $root 'docs/app-performance-results.json')
    $result | ConvertTo-Json -Depth 5
} finally {
    if (!$appProcess.HasExited) {
        & python "$PSScriptRoot/app_client.py" --data-dir $directory quit | Out-Null
        if (!$appProcess.WaitForExit(5000)) { Stop-Process -Id $appProcess.Id }
    }
}
