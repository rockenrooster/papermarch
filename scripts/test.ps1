# Builds and runs every check that needs no device (TECH_SPEC 8.4). Exit 0 = all passed.
#   scripts\test.ps1 [-Config debug|dev] [-NoBuild] [-Quick]
# -Quick skips the long headless runs (the bake acceptance and the soak).
param(
    [ValidateSet('debug', 'dev')][string]$Config = 'dev',
    [switch]$NoBuild,
    [switch]$Quick
)
. "$PSScriptRoot\env.ps1"
if (-not $NoBuild) { & "$PSScriptRoot\build-desktop.ps1" -Config $Config }
$bin = Join-Path $GT_ROOT "build\desktop-$Config"
$out = Join-Path $GT_ROOT 'captures\tests'
New-Item -ItemType Directory -Force $out | Out-Null
$failed = @()

function Step([string]$Name, [scriptblock]$Body) {
    Write-Host "[test] $Name"
    & $Body
    if ($LASTEXITCODE -ne 0) { $script:failed += $Name; Write-Host "[test] FAILED: $Name ($LASTEXITCODE)" }
}

Step 'unit tests' { & (Join-Path $bin 'glowtide_tests.exe') $out }
Step 'png opens in PIL' { python -I -B -c "import sys; from PIL import Image; im = Image.open(sys.argv[1]); im.load(); assert im.size == (3, 2) and im.getpixel((2, 1))[0] == 200, im.getpixel((2, 1))" (Join-Path $out 'test_png.png') }
$headless = Join-Path $bin 'glowtide_headless.exe'
Step 'determinism, levels 1-5' { & $headless --sim determinism --from 1 --to 5 --seconds 60 }
Step 'determinism, the hash levels (geode levels, each kind, each recipe introduction, a set piece, a replay)' {
    & $headless --sim determinism --hash-levels --seconds 60
}
Step 'variety: the no-repeat rule and the counts on the baked table (VARIETY_PLAN 5)' { & $headless --sim variety --from 1 --to 600 }
if (-not $Quick) {
    Step 'acceptance, levels 1-5' { & $headless --sim accept --from 1 --to 5 }
    Step 'soak, levels 1-5 (greedy, sweep)' { & $headless --sim soak --from 1 --to 5 --bots greedy,sweep }
}
if ($failed.Count) { Write-Host "[test] $($failed.Count) failed: $($failed -join ', ')"; exit 1 }
Write-Host "[test] all passed"
exit 0
