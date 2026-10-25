$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
& python "$PSScriptRoot/patch-nim-ui.py"
if ($LASTEXITCODE) { throw 'Pinned native focus patch failed.' }
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio 2022 C++ Build Tools are required.' }
$cmake = "$vs/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
$build = "$root/.tools/nim-duilib/build"
& $cmake -S nim-ui -B $build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE) { throw 'Configure failed. Run scripts/bootstrap-nim-ui.ps1 first.' }
& $cmake --build $build --config Release --parallel 8 --target DesktopTodoNim
if ($LASTEXITCODE) { throw 'Build failed.' }
$destination = "$root/dist/nim-ui"
New-Item $destination -ItemType Directory -Force | Out-Null
function Copy-ChangedFile($source, $target) {
    if ((Test-Path -LiteralPath $target) -and ((Get-FileHash -LiteralPath $source).Hash -eq (Get-FileHash -LiteralPath $target).Hash)) { return }
    New-Item (Split-Path $target -Parent) -ItemType Directory -Force | Out-Null
    try { Copy-Item -LiteralPath $source -Destination $target -Force }
    catch {
        if ([IO.Path]::GetExtension($target) -ne '.exe') { throw }
        # Windows allows renaming an active image. Preserve it in the ignored
        # build directory so the running app and its data are not interrupted.
        $previous = "$root/.tools/nim-duilib/build/running-$([Guid]::NewGuid()).exe"
        Move-Item -LiteralPath $target -Destination $previous
        try { Copy-Item -LiteralPath $source -Destination $target -Force }
        catch { Move-Item -LiteralPath $previous -Destination $target; throw }
    }
}
Copy-ChangedFile "$root/.tools/nim-duilib/bin/Release/DesktopTodo-Nim.exe" "$destination/DesktopTodo-Nim.exe"
$resources = "$root/.tools/nim-duilib/source/bin/resources"
foreach ($file in Get-ChildItem $resources -File -Recurse) {
    $relative = [IO.Path]::GetRelativePath($resources, $file.FullName)
    Copy-ChangedFile $file.FullName "$destination/resources/$relative"
}
New-Item "$destination/resources/themes/default/todo" -ItemType Directory -Force | Out-Null
foreach ($file in Get-ChildItem "$root/nim-ui/resources" -File) {
    Copy-ChangedFile $file.FullName "$destination/resources/themes/default/todo/$($file.Name)"
}
New-Item "$destination/licenses" -ItemType Directory -Force | Out-Null
Copy-Item "$root/.tools/nim-duilib/source/LICENSE" "$destination/licenses/nim-duilib.txt" -Force
Copy-Item "$root/.tools/nim-duilib/skia/LICENSE" "$destination/licenses/skia.txt" -Force
Write-Host "Built $destination/DesktopTodo-Nim.exe (keep resources beside it)."
