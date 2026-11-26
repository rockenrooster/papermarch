# Builds and runs every check that needs no device (TECH_SPEC 9.4). Exit 0 = all passed.
#   scripts\test.ps1 [-Config debug|dev] [-NoBuild] [-Quick]
# -Quick skips the flow test and the table report.
param(
    [ValidateSet('debug', 'dev')][string]$Config = 'dev',
    [switch]$NoBuild,
    [switch]$Quick
)
. "$PSScriptRoot\env.ps1"
if (-not $NoBuild) { & "$PSScriptRoot\build-desktop.ps1" -Config $Config }
$bin = Join-Path $PM_ROOT "build\desktop-$Config"
$out = Join-Path $PM_ROOT 'captures\tests'
New-Item -ItemType Directory -Force $out | Out-Null
$failed = @()

function Step([string]$Name, [scriptblock]$Body) {
    Write-Host "[test] $Name"
    & $Body
    if ($LASTEXITCODE -ne 0) { $script:failed += $Name; Write-Host "[test] FAILED: $Name ($LASTEXITCODE)" }
}

Step 'unit tests (the battle rules, the text form, the bot on every level, palettes, save)' { & (Join-Path $bin 'papermarch_tests.exe') $out }
Step 'png opens in PIL' { python -I -B -c "import sys; from PIL import Image; im = Image.open(sys.argv[1]); im.load(); assert im.size == (3, 2) and im.getpixel((2, 1))[0] == 200, im.getpixel((2, 1))" (Join-Path $out 'test_png.png') }
$headless = Join-Path $bin 'papermarch_headless.exe'
Step 'the table: the bot wins every level as baked' { & $headless --sim table | Select-Object -Last 3 }
if (-not $Quick) {
    $exe = Join-Path $bin 'papermarch.exe'
    Step 'the flow test (drag to march, win, NEXT, LEVELS, defeat, RETRY, pause, settings)' { & $exe --flow-test --fixed-step --silent --offscreen --no-audio }
    Step 'the layout lint on every screen' {
        $bad = 0
        foreach ($s in 'screen_play', 'screen_pause', 'screen_settings', 'screen_levels', 'screen_end', 'screen_win', 'screen_lose', 'home_update') {
            foreach ($res in '1080x2400', '540x1200', '1200x1920', '1080x1440') {
                & $exe --picture $s --lint --frames 90 --res $res --silent --offscreen --no-audio | Out-Null
                if ($LASTEXITCODE -ne 0) { Write-Host "[test] lint failed: $s at $res"; $bad++ }
            }
        }
        $global:LASTEXITCODE = [int]($bad -gt 0)
    }
}
if ($failed.Count) { Write-Host "[test] $($failed.Count) failed: $($failed -join ', ')"; exit 1 }
Write-Host "[test] all passed"
exit 0
