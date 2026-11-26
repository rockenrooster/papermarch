# The device matrix (P6 task 11) on one device: every device check in a row, results in captures\matrix\<id>.md.
#   scripts\matrix-android.ps1 -Serial tablet|op12r|moto|<serial> [-NoInstall] [-Pictures concept,geodes]
# Runs: install, flow test, the device picture checks, back key, save through a force-stop, cold start, 20 lifecycle cycles, bench_flood,
# bench_play on the heaviest level (87), and reads the panel's refresh rate and the mode the game asked for.
# Each step takes the device lock and checks the foreground app itself (PLAN 3.3); a step that fails does not stop
# the others.
param(
    [Parameter(Mandatory)][string]$Serial,
    [switch]$NoInstall,
    [string[]]$Pictures = @('concept', 'world')  # device picture checks (scripts\picture-android.ps1): a battle on level 12, the first level of world 1
)
. "$PSScriptRoot\env.ps1"

$Serial = Resolve-PmSerial $Serial
Assert-PmDevice $Serial
$id = Get-PmDeviceId $Serial
$model = "$(& $PM_ADB -s $Serial shell getprop ro.product.model)".Trim()
$android = "$(& $PM_ADB -s $Serial shell getprop ro.build.version.release)".Trim()
$size = "$((& $PM_ADB -s $Serial shell wm size) | Select-Object -Last 1)".Trim() -replace '.*:\s*', ''
$modes = (& $PM_ADB -s $Serial shell dumpsys display) | Select-String 'fps=([\d.]+)' -AllMatches |
    ForEach-Object { $_.Matches } | ForEach-Object { [double]$_.Groups[1].Value } | Sort-Object -Unique
$outDir = Join-Path $PM_ROOT 'captures\matrix'
New-Item -ItemType Directory -Force $outDir | Out-Null
$report = Join-Path $outDir "$id.md"
$rows = @()
function Add-Row([string]$check, [bool]$ok, [string]$detail) {
    $script:rows += "| $check | $(if ($ok) { 'PASS' } else { 'FAIL' }) | $detail |"
    Write-Host "[matrix] $check -> $(if ($ok) { 'PASS' } else { 'FAIL' }): $detail"
}
function Invoke-Step([string]$script, [string[]]$argv) {
    $out = & pwsh -NoProfile -File (Join-Path $PSScriptRoot $script) @argv 2>&1 | ForEach-Object { "$_" }
    return @{ code = $LASTEXITCODE; out = $out }
}

Write-Host "[matrix] $model ($id), Android $android, $size, panel modes: $($modes -join ', ') Hz"
# A phone that sleeps locks (and then nothing can come to the front): start awake and unlocked, and stay awake.
Assert-PmUnlocked $Serial
$keepAwake = Start-PmKeepAwake $Serial
try {
if (-not $NoInstall) {
    $r = Invoke-Step 'deploy-android.ps1' @('-Serial', $Serial, '-NoBuild', '-NoLaunch')
    Add-Row 'Install the dev APK' ($r.code -eq 0) "$(($r.out | Select-Object -Last 1))"
}
$r = Invoke-Step 'flow-android.ps1' @('-Serial', $Serial, '-NoInstall')
Add-Row 'Flow test' ($r.code -eq 0) "$(($r.out | Where-Object { $_ -match '\[flow\]' } | Select-Object -Last 1))"
$r = Invoke-Step 'simhash-android.ps1' @('-Serial', $Serial)
Add-Row 'Determinism against the PC (the hash levels: kHashLevels)' ($r.code -eq 0) "$(($r.out | Where-Object { $_ -match '\[simhash\] (PASS|FAIL)' } | Select-Object -Last 1))"
# The phone draws what the PC draws (VARIETY_TECH 12, trap 1: a phone once passed everything with no crowd on screen).
foreach ($p in $Pictures) {
    $r = Invoke-Step 'picture-android.ps1' @('-Serial', $Serial, '-Picture', $p)
    Add-Row "Picture $p against the PC" ($r.code -eq 0) "$(($r.out | Where-Object { $_ -match 'gold .*violet' } | Select-Object -Last 1))"
}
$r = Invoke-Step 'backkey-android.ps1' @('-Serial', $Serial)
Add-Row 'Back key (play -> pause -> play, HOME leaves)' ($r.code -eq 0) "$(($r.out | Where-Object { $_ -match '\[back\]' }) -join '; ')"
$r = Invoke-Step 'save-android.ps1' @('-Serial', $Serial)
Add-Row 'Save through a force-stop' ($r.code -eq 0) "$(($r.out | Where-Object { $_ -match 'after force-stop' } | Select-Object -Last 1))"
$r = Invoke-Step 'coldstart-android.ps1' @('-Serial', $Serial, '-Runs', '3')
Add-Row 'Cold start (target 2 s)' ($r.code -eq 0) "$(($r.out | Where-Object { $_ -match 'median' } | Select-Object -Last 1))"
$r = Invoke-Step 'lifecycle-android.ps1' @('-Serial', $Serial, '-Cycles', '20', '-AppArgs', '--level 8 --bot greedy')
Add-Row 'Lifecycle, 20 cycles during a level' ($r.code -eq 0) "$(($r.out | Where-Object { $_ -match '\[lifecycle\]' } | Select-Object -Last 1))"
foreach ($b in @(@('bench_flood', @('-Seconds', '60', '-Warmup', '60')), @('bench_play', @('-Seconds', '90', '-Warmup', '5', '-AppArgs', '--level 87')))) {
    $r = Invoke-Step 'bench-android.ps1' (@('-Serial', $Serial, '-Scenario', $b[0]) + $b[1])
    $line = ($r.out | Where-Object { $_ -match "$($b[0]):" } | Select-Object -Last 1)
    Add-Row "$($b[0])$(if ($b[0] -eq 'bench_play') { ' level 87' })" ($r.code -eq 0 -and $line -match 'PASS') "$line"
}
# The refresh rate the game asked for and got (TECH_SPEC 5.10): the activity's log line and the benchmark's figure.
$display = (& $PM_ADB -s $Serial logcat -d -s PAPERMARCH:I PMLIFE:I) | Select-String 'display: mode|refresh:' | Select-Object -Last 2
Add-Row 'Refresh rate' $true "panel modes $($modes -join '/') Hz; $(($display | ForEach-Object { "$_" -replace '^.*?: ', '' }) -join '; ')"

} finally {
    Stop-PmKeepAwake $keepAwake
}

$text = @("# Device matrix: $model ($id)", '', "Android $android, $size, panel modes $($modes -join ', ') Hz. $(Get-Date -Format 'yyyy-MM-dd HH:mm').",
          '', '| Check | Result | Detail |', '|---|---|---|') + $rows
$text | Set-Content -Encoding utf8 $report
Write-Host "[matrix] $report"
if ($rows | Where-Object { $_ -match '\| FAIL \|' }) { exit 1 }
exit 0
