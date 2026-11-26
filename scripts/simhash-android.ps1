# Determinism across machines (TECH_SPEC 4.2): the phone's hashes after 60 s of the greedy bot on the hash levels (levels
# 1-5, geode levels, one of each kind and recipe, a set piece, a replay: kHashLevels in src\sim\level.h; --scenario
# sim_hash) against the PC's (glowtide_headless --sim determinism --hash-levels). Exit 0 when every one is equal.
#   scripts\simhash-android.ps1 -Serial moto|tablet|op12r|<serial> [-Install] [-Config dev]
param(
    [Parameter(Mandatory)][string]$Serial,
    [switch]$Install,
    [ValidateSet('debug', 'dev')][string]$Config = 'dev',
    [int]$TimeoutSeconds = 900
)
. "$PSScriptRoot\env.ps1"
# GT_PC_BUILD: a frozen copy of a desktop build folder, so a phone is checked against the build its APK came from while
# the source moves on.
$pcBuild = if ($env:GT_PC_BUILD) { $env:GT_PC_BUILD } else { Join-Path $GT_ROOT "build\desktop-$Config" }
$headless = Join-Path $pcBuild 'glowtide_headless.exe'
$pcHash = @{}
$levels = @()  # in the PC's order
foreach ($l in @(& $headless --sim determinism --hash-levels --seconds 60)) { if ($l -match 'determinism level (\d+): \w+ \(hash ([0-9a-f]+)') { $pcHash[[int]$Matches[1]] = $Matches[2]; $levels += [int]$Matches[1] } }
if (-not $pcHash.Count) {  # a build from before V4 (GT_PC_BUILD frozen) knows only its fixed list
    foreach ($lv in 1, 2, 3, 4, 5, 6, 11, 23, 47, 10, 15, 21, 31, 41) {
        if ("$(& $headless --sim determinism --level $lv --seconds 60)" -match 'hash ([0-9a-f]+)') { $pcHash[$lv] = $Matches[1]; $levels += $lv }
    }
}
if (-not $levels.Count) { throw "no PC hashes from $headless" }
$Serial = Resolve-GtSerial $Serial
Assert-GtDevice $Serial
$lock = Enter-GtDeviceLock $Serial 'glowtide-simhash'
$awake = $null
$log = @()
try {
    if ($Install) {
        $apk = if ($env:GT_APK) { $env:GT_APK } else { Join-Path $GT_ROOT 'android\app\build\outputs\apk\dev\app-dev.apk' }
        & $GT_ADB -s $Serial install -r -t $apk | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "install failed on $Serial" }
    }
    if (-not (Wait-GtDeviceIdle $Serial -QuietSeconds 2 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
    Assert-GtUnlocked $Serial
    $awake = Start-GtKeepAwake $Serial
    $appPid = Start-GtApp $Serial '--scenario sim_hash'
    if (-not $appPid) { throw "GLOWTIDE did not start on $Serial" }
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $log = @(& $GT_ADB -s $Serial logcat -d --pid=$appPid -s GTSIM:I)
        if (@($log | Where-Object { $_ -match 'determinism level (\d+): hash' }).Count -ge $levels.Count) { break }
        if (-not (Get-GtPid $Serial)) { Start-Sleep -Seconds 1; $log = @(& $GT_ADB -s $Serial logcat -d --pid=$appPid -s GTSIM:I); break }
        Start-Sleep -Seconds 2
    }
    Stop-GtApp $Serial
} finally {
    Stop-GtKeepAwake $awake
    Exit-GtDeviceLock $lock
}
$phone = @{}
foreach ($l in $log) { if ($l -match 'determinism level (\d+): hash ([0-9a-f]+)') { $phone[[int]$Matches[1]] = $Matches[2] } }
$bad = 0
foreach ($lv in $levels) {
    $ok = $phone.ContainsKey($lv) -and $phone[$lv] -eq $pcHash[$lv]
    if (-not $ok) { ++$bad }
    Write-Host ("[simhash] level {0}: phone {1}, PC {2} {3}" -f $lv, $phone[$lv], $pcHash[$lv], $(if ($ok) { 'equal' } else { 'DIFFER' }))
}
if ($bad) { Write-Host "[simhash] FAIL: $bad of $($levels.Count) differ"; exit 1 }
Write-Host "[simhash] PASS: $($levels.Count) levels equal"
exit 0
