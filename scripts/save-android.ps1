# Save survives a force-stop in the middle of a level (P3 exit check). Seeds a save through run-as (debuggable
# builds), launches, starts the level with a touch, force-stops it mid-level, launches again and compares what the game
# logs it loaded. The device's own save is put back afterwards.
#   scripts\save-android.ps1 -Serial moto|tablet|op12r|<serial>
param([Parameter(Mandatory)][string]$Serial)
. "$PSScriptRoot\env.ps1"

$Serial = Resolve-PmSerial $Serial
Assert-PmDevice $Serial
$lock = Enter-PmDeviceLock $Serial "papermarch-save"
$remote = 'files/save.txt'
$tmp = Join-Path ([IO.Path]::GetTempPath()) "papermarch_save_$PID"
New-Item -ItemType Directory -Force $tmp | Out-Null
$seed = Join-Path $tmp 'seed.txt'
$backup = Join-Path $tmp 'backup.txt'
$want = 'level 7, 16 stars, sound 1 music 0 haptics 1 auto-fire 1'
$fail = 0
# exec-in / exec-out move the bytes unchanged (no shell quoting in between).
function Push-Save([string]$local) {
    cmd /c "`"$PM_ADB`" -s $Serial exec-in run-as $PM_APP_ID sh -c `"mkdir -p files; cat > $remote`" < `"$local`""
}
function Get-SaveLine([string]$appPid) {
    return "$(& $PM_ADB -s $Serial logcat -d --pid=$appPid -s PAPERMARCH:I | Select-String 'save: ' | Select-Object -Last 1)"
}
$hadSave = $false
try {
    if (-not (Wait-PmDeviceIdle $Serial -QuietSeconds 2 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
    Stop-PmApp $Serial
    cmd /c "`"$PM_ADB`" -s $Serial exec-out run-as $PM_APP_ID cat $remote > `"$backup`" 2>nul"
    $hadSave = (Test-Path $backup) -and (Get-Item $backup).Length -gt 0 -and -not (Select-String -Path $backup -Pattern 'No such file' -Quiet)
    [IO.File]::WriteAllText($seed, "version=1`nlevel=7`nstars=3332221`nsound=1`nmusic=0`nhaptics=1`nautofire=1`nreduce_effects=0`nrefresh60=0`nhints=3`n")
    Push-Save $seed
    $appPid = Start-PmApp $Serial ''
    Start-Sleep -Seconds 3
    $first = Get-SaveLine $appPid
    Write-Host "[save] first launch: $($first -replace '^.*save: ', '')"
    if ($first -notmatch [regex]::Escape($want)) { ++$fail }
    # Start the level (a touch on HOME), hold for two seconds, force-stop in the middle of it.
    if ((Get-PmForegroundPackage $Serial) -ne $PM_APP_ID) { throw "PAPERMARCH is not in front" }
    $size = ((& $PM_ADB -s $Serial shell wm size) | Select-Object -Last 1) -replace '.*:\s*', ''
    $w, $h = $size.Trim() -split 'x'
    $x = [int]([int]$w / 2); $y = [int]([int]$h * 3 / 4)
    & $PM_ADB -s $Serial shell input swipe $x $y ($x + 60) $y 2000 | Out-Null
    Stop-PmApp $Serial
    Start-Sleep -Seconds 1
    $appPid = Start-PmApp $Serial ''
    Start-Sleep -Seconds 3
    $second = Get-SaveLine $appPid
    Write-Host "[save] after force-stop: $($second -replace '^.*save: ', '')"
    if ($second -notmatch [regex]::Escape($want)) { ++$fail }
    Stop-PmApp $Serial
} finally {
    if ($hadSave) { Push-Save $backup } else { & $PM_ADB -s $Serial shell "run-as $PM_APP_ID rm -f $remote" | Out-Null }
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    Exit-PmDeviceLock $lock
}
if ($fail) { Write-Host "[save] FAIL"; exit 1 }
Write-Host "[save] PASS on $Serial"
exit 0
