# Builds PAPERMARCH for a browser: Emscripten (WebAssembly, WebGL 2) from the shared emsdk, CMake + Ninja.
#   scripts\build-web.ps1 [-Clean] [-Emsdk <folder>]
# Output: build\web\site\ (index.html, index.js, index.wasm, the manifest and the icons): a static site that
# scripts\serve-web.ps1 serves. The emsdk is D:\Stuff\DevFolder\emsdk (shared with RAVEFORM; this script only reads it).
param(
    [switch]$Clean,
    [string]$Emsdk = 'D:\Stuff\DevFolder\emsdk'
)
. "$PSScriptRoot\env.ps1"

# The emsdk's own config names its node, python and clang; its launchers need only that and their folder on PATH.
$emscripten = Join-Path $Emsdk 'upstream\emscripten'
if (-not (Test-Path (Join-Path $emscripten 'emcmake.exe'))) { throw "no Emscripten in $Emsdk (emsdk install latest)" }
$env:EMSDK = $Emsdk
$env:EM_CONFIG = Join-Path $Emsdk '.emscripten'
$env:PATH = "$emscripten;$env:PATH"
$emcmake = Join-Path $emscripten 'emcmake.exe'
$ninja = (Get-Command ninja -ErrorAction Stop).Source

$dir = Join-Path $PM_ROOT 'build\web'
if ($Clean -and (Test-Path $dir)) { Remove-Item -Recurse -Force $dir }
# Configured every time (a few seconds): whether assets/art exists decides the preloaded files (CMakeLists.txt).
& $emcmake $PM_CMAKE -S $PM_ROOT -B $dir -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_MAKE_PROGRAM=$ninja" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
& $PM_CMAKE --build $dir --parallel $PM_JOBS --target papermarch
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

$site = Join-Path $dir 'site'
New-Item -ItemType Directory -Force $site | Out-Null
foreach ($f in 'index.html', 'index.js', 'index.wasm') { Copy-Item (Join-Path $dir $f) $site -Force }
# The art's preloaded files (assets/art), when there is art.
$data = Join-Path $dir 'index.data'
if (Test-Path $data) { Copy-Item $data $site -Force } else { Remove-Item (Join-Path $site 'index.data') -ErrorAction SilentlyContinue }
foreach ($f in Get-ChildItem (Join-Path $PM_ROOT 'web') -File | Where-Object { $_.Name -ne 'shell.html' }) { Copy-Item $f.FullName $site -Force }
$wasm = (Get-Item (Join-Path $site 'index.wasm')).Length
Write-Host ("[build] {0} (index.wasm {1:N1} MB)" -f $site, ($wasm / 1MB))
