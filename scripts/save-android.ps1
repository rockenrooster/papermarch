# Save survives a force-stop in the middle of a level (P3 exit check). Seeds a save through run-as (debuggable
# builds), launches, starts the level with a touch, force-stops it mid-level, launches again and compares what the game
# logs it loaded. The device's own save is put back afterwards.
#   scripts\save-android.ps1 -Serial moto|tablet|op12r|<serial>
param([Parameter(Mandatory)][string]$Serial)
. "$PSScriptRoot\env.ps1"

$Serial = Resolve-GtSerial $Serial
Assert-GtDevice $Serial
$lock = Enter-GtDeviceLock $Serial "glowtide-save"
$remote = 'files/save.txt'
$tmp = Join-Path ([IO.Path]::GetTempPath()) "glowtide_save_$PID"
New-Item -ItemType Directory -Force $tmp | Out-Null
$seed = Join-Path $tmp 'seed.txt'
$backup = Join-Path $tmp 'backup.txt'
$want = 'level 7, 16 stars, sound 1 music 0 haptics 1 auto-fire 1'
$fail = 0
# exec-in / exec-out move the bytes unchanged (no shell quoting in between).
function Push-Save([string]$local) {
    cmd /c "`"$GT_ADB`" -s $Serial exec-in run-as $GT_APP_ID sh -c `"mkdir -p files; cat > $remote`" < `"$local`""
}
function Get-SaveLine([string]$appPid) {
    return "$(& $GT_ADB -s $Serial logcat -d --pid=$appPid -s GLOWTIDE:I | Select-String 'save: ' | Select-Object -Last 1)"
}
$hadSave = $false
try {
    if (-not (Wait-GtDeviceIdle $Serial -QuietSeconds 2 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
    Stop-GtApp $Serial
    cmd /c "`"$GT_ADB`" -s $Serial exec-out run-as $GT_APP_ID cat $remote > `"$backup`" 2>nul"
    $hadSave = (Test-Path $backup) -and (Get-Item $backup).Length -gt 0 -and -not (Select-String -Path $backup -Pattern 'No such file' -Quiet)
    [IO.File]::WriteAllText($seed, "version=1`nlevel=7`nstars=3332221`nsound=1`nmusic=0`nhaptics=1`nautofire=1`nreduce_effects=0`nrefresh60=0`nhints=3`n")
    Push-Save $seed
    $appPid = Start-GtApp $Serial ''
    Start-Sleep -Seconds 3
    $first = Get-SaveLine $appPid
    Write-Host "[save] first launch: $($first -replace '^.*save: ', '')"
    if ($first -notmatch [regex]::Escape($want)) { ++$fail }
    # Start the level (a touch on HOME), hold for two seconds, force-stop in the middle of it.
    if ((Get-GtForegroundPackage $Serial) -ne $GT_APP_ID) { throw "GLOWTIDE is not in front" }
    $size = ((& $GT_ADB -s $Serial shell wm size) | Select-Object -Last 1) -replace '.*:\s*', ''
    $w, $h = $size.Trim() -split 'x'
    $x = [int]([int]$w / 2); $y = [int]([int]$h * 3 / 4)
    & $GT_ADB -s $Serial shell input swipe $x $y ($x + 60) $y 2000 | Out-Null
    Stop-GtApp $Serial
    Start-Sleep -Seconds 1
    $appPid = Start-GtApp $Serial ''
    Start-Sleep -Seconds 3
    $second = Get-SaveLine $appPid
    Write-Host "[save] after force-stop: $($second -replace '^.*save: ', '')"
    if ($second -notmatch [regex]::Escape($want)) { ++$fail }
    Stop-GtApp $Serial
} finally {
    if ($hadSave) { Push-Save $backup } else { & $GT_ADB -s $Serial shell "run-as $GT_APP_ID rm -f $remote" | Out-Null }
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    Exit-GtDeviceLock $lock
}
if ($fail) { Write-Host "[save] FAIL"; exit 1 }
Write-Host "[save] PASS on $Serial"
exit 0
