$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
& "$PSScriptRoot/build-nim-ui.ps1"
$saved = $env:DESKTOP_TODO_TEST_EXE
try {
    $env:DESKTOP_TODO_TEST_EXE = "$root/dist/nim-ui/DesktopTodo-Nim.exe"
    foreach ($suite in @('test-app-integration.py', 'test-storage-management.py', 'test-wheel-regression.py', 'test-native-focus.py', 'test-drag-motion.py', 'test-locked-wheel.py')) {
        & python "$PSScriptRoot/$suite"
        if ($LASTEXITCODE) { throw "$suite failed: $LASTEXITCODE" }
    }
} finally {
    $env:DESKTOP_TODO_TEST_EXE = $saved
}
