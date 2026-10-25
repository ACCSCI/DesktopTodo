$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $root '.tools/web-comparison'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
function Fetch-Pinned($url, $name, $digest) {
    $target = Join-Path $destination $name
    if (!(Test-Path -LiteralPath $target)) { Invoke-WebRequest $url -OutFile $target }
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $digest) {
        throw "Digest mismatch: $name"
    }
    return $target
}
$wrapper = Fetch-Pinned 'https://codeload.github.com/webview/webview/zip/refs/tags/0.12.0' 'webview-0.12.0.zip' '446EC9730AD4A1574F5364A62EC5989E78C8A59D0497082E6FBF6B423654A9BB'
if (!(Test-Path "$destination/webview-0.12.0")) { Expand-Archive $wrapper $destination }
$sdk = Fetch-Pinned 'https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.4258.31' 'webview2-1.0.4258.31.zip' '56F7F4B8BF9AEE4B8EFEFBBDD4F67D5F74EBD1B100ED0806DA71BF76AF481AA9'
if (!(Test-Path "$destination/sdk")) { Expand-Archive $sdk "$destination/sdk" }
$null = Fetch-Pinned 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp' 'json.hpp' 'AAF127C04CB31C406E5B04A63F1AE89369FCCDE6D8FA7CDDA1ED4F32DFC5DE63'
npm.cmd ci --prefix "$root/web-comparison" --no-audit --no-fund
if ($LASTEXITCODE -ne 0) { throw 'Frontend dependency install failed' }
