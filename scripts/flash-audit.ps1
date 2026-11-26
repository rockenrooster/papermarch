# The flashing audit (P6 task 5, GAME_DESIGN 14): renders every frame of the win, boss win, lose, core-hit, geode, WARD, KINDLE and COMET strips,
# with and without REDUCE EFFECTS, and counts brightness swings of more than 10% per second (at most three).
#   scripts\flash-audit.ps1 [-Config debug|dev] [-NoBuild]
param(
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [switch]$NoBuild
)
. "$PSScriptRoot\env.ps1"
if (-not $NoBuild) { & "$PSScriptRoot\build-desktop.ps1" -Config $Config }
$exe = Join-Path $PM_ROOT "build\desktop-$Config\papermarch.exe"
$root = Join-Path $PM_ROOT 'captures\flash'
$strips = [ordered]@{ win_strip = 210; boss_win_strip = 210; lose_strip = 150; core_hit_strip = 120; geode_strip = 60; ward_strip = 60
            kindle_strip = 60; comet_strip = 60; twist_eclipse = 150; dawn_strip = 120; colossus_fall_strip = 90 }
$failed = 0
foreach ($mode in 'normal', 'reduced') {
    $dir = Join-Path $root $mode
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    New-Item -ItemType Directory -Force $dir | Out-Null
    foreach ($name in $strips.Keys) {
        $n = $strips[$name]
        $argv = @('--picture', $name, '--strip', "$n", '--strip-step', '1', '--frames', "$n", '--res', '270x600',
                  '--screenshot', (Join-Path $dir "$name.png"), '--silent', '--offscreen')
        if ($mode -eq 'reduced') { $argv += '--reduce-effects' }
        & $exe @argv | Out-Null
        if ($LASTEXITCODE -ne 0) { Write-Host "[flash] $mode ${name}: run failed ($LASTEXITCODE)"; ++$failed; continue }
        Write-Host -NoNewline "[$mode] "
        python -I -B (Join-Path $PM_ROOT 'tools\flash_audit.py') $name $dir
        if ($LASTEXITCODE -ne 0) { ++$failed }
    }
}
if ($failed) { Write-Host "[flash] $failed failed"; exit 1 }
Write-Host "[flash] PASS"
exit 0
