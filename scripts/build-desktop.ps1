# Builds the Windows dev build: native CMake + Ninja + llvm-mingw clang from PATH (TECH_SPEC 2.2).
#   scripts\build-desktop.ps1 [-Config debug|dev] [-Clean] [-Sanitize]
# Output: build\desktop-<config>\glowtide.exe, glowtide_headless.exe, glowtide_tests.exe
# debug = Debug with a GL debug context; dev = RelWithDebInfo; -Sanitize = build\desktop-asan (tests and headless only).
param(
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [switch]$Clean,
    [switch]$Sanitize
)
. "$PSScriptRoot\env.ps1"

$buildType = @{ debug = 'Debug'; dev = 'RelWithDebInfo' }[$Config]
$dir = Join-Path $GT_ROOT "build\desktop-$Config"
$extra = @()
if ($Sanitize) {
    $dir = Join-Path $GT_ROOT 'build\desktop-asan'
    $extra = @('-DGT_SANITIZE=ON', '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
               '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
               '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined')
}
if ($Clean -and (Test-Path $dir)) { Remove-Item -Recurse -Force $dir }

$clang = (Get-Command clang -ErrorAction Stop).Source
$clangxx = (Get-Command clang++ -ErrorAction Stop).Source
$ninja = (Get-Command ninja -ErrorAction Stop).Source

if (-not (Test-Path (Join-Path $dir 'build.ninja'))) {
    & $GT_CMAKE -S $GT_ROOT -B $dir -G Ninja "-DCMAKE_BUILD_TYPE=$buildType" "-DCMAKE_MAKE_PROGRAM=$ninja" `
        "-DCMAKE_C_COMPILER=$clang" "-DCMAKE_CXX_COMPILER=$clangxx" @extra
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
}
$targets = @()
if ($Sanitize) { $targets = @('--target', 'glowtide_tests', 'glowtide_headless') }
& $GT_CMAKE --build $dir --parallel $GT_JOBS @targets
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
Write-Host "[build] $dir"
