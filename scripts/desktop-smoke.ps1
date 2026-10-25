$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$probeDir = Join-Path $root 'window-probe'
$cli = Join-Path $root '.tools/node_modules/@native-sdk/cli/bin/native.js'
$exe = Join-Path $probeDir 'zig-out/bin/window-probe.exe'
$control = Join-Path $root 'scripts/probe-control.ps1'
$inspect = Join-Path $root 'scripts/inspect-windows.py'

function Read-ProbeFacts {
    $result = & python $inspect --pid $probe.Id
    if ($LASTEXITCODE -ne 0) { throw 'Native inspection failed' }
    $facts = ($result -join "`n") | ConvertFrom-Json
    $facts.windows = @($facts.windows | Where-Object class -eq 'NativeSdkWindowsHost')
    return $facts
}
function Require-Fact($condition, [string]$message) {
    if (!$condition) { throw $message }
}
function Note-From($facts) {
    $notes = @($facts.windows | Where-Object title -eq 'Todo Overlay Probe')
    Require-Fact ($notes.Count -eq 1) 'Expected exactly one native note window'
    return $notes[0]
}

Push-Location $probeDir
$probe = Start-Process -FilePath $exe -WorkingDirectory $probeDir -WindowStyle Hidden `
    -RedirectStandardOutput 'desktop.stdout.log' -RedirectStandardError 'desktop.stderr.log' -PassThru
try {
    & node $cli automate assert "publisher_pid=$($probe.Id)" 'role=button name="Desktop"'
    if ($LASTEXITCODE -ne 0) { throw 'Probe startup failed' }
    & $control -Name Desktop
    & $control -Name 'Overlay probe'
    $desktop = Read-ProbeFacts
    $note = Note-From $desktop
    $unlock = @($desktop.windows | Where-Object title -eq 'Todo Unlock Probe')
    Require-Fact ($unlock.Count -eq 1) 'Expected one unlock control'
    Require-Fact ($note.parent -ne 0) 'Desktop note has no parent'
    Require-Fact ($note.parentClass -in @('Progman', 'WorkerW')) 'Unexpected desktop host class'
    Require-Fact ($unlock[0].parent -eq $note.parent) 'Unlock control escaped the desktop layer'
    Require-Fact (($note.style -band 0x40000000) -ne 0) 'Desktop note lacks WS_CHILD'
    Require-Fact (($note.extended -band 8) -eq 0) 'Desktop note is still topmost'
    Require-Fact (($note.extended -band 0x20) -ne 0) 'Locked body lacks transparent input style'
    Require-Fact (($unlock[0].extended -band 0x20) -eq 0) 'Unlock control is click-through'

    & $control -Name Unlock -View unlock-canvas
    $unlocked = Read-ProbeFacts
    $noteUnlocked = Note-From $unlocked
    Require-Fact ($noteUnlocked.parent -eq $note.parent) 'Unlock lost desktop attachment'
    Require-Fact (($noteUnlocked.extended -band 0x20) -eq 0) 'Unlock retained transparent input style'
    & $control -Name Topmost
    $topmost = Read-ProbeFacts
    $noteTopmost = Note-From $topmost
    Require-Fact ($noteTopmost.parent -eq 0) 'Topmost note is still a desktop child'
    Require-Fact (($noteTopmost.extended -band 8) -ne 0) 'Topmost mode lacks native topmost style'
    & $control -Name Window
    $normal = Read-ProbeFacts
    $noteNormal = Note-From $normal
    Require-Fact ($noteNormal.parent -eq 0) 'Ordinary note is still a child'
    Require-Fact (($noteNormal.extended -band 8) -eq 0) 'Ordinary note is still topmost'
    Require-Fact (($noteNormal.style -band 0x00C00000) -eq 0x00C00000) 'Ordinary window lacks caption'
    [ordered]@{ desktop = $desktop; unlocked = $unlocked; topmost = $topmost; normal = $normal } |
        ConvertTo-Json -Depth 8 | Set-Content (Join-Path $root 'docs/desktop-native-facts.json')
    Write-Output 'Desktop parent, mode transitions, native styles and unlock reconstruction passed.'
} finally {
    $running = Get-Process -Id $probe.Id -ErrorAction SilentlyContinue
    if ($running -and $running.Path -eq $exe) { Stop-Process -Id $probe.Id }
    Pop-Location
}
