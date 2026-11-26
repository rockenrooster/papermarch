# The system back key (GAME_DESIGN 4): play -> pause -> play, and on HOME it leaves the app. Sends BACK only while
# GLOWTIDE is in front (PLAN 3.3) and reads the screen changes the game logs (GTLIFE).
#   scripts\backkey-android.ps1 -Serial moto|tablet|op12r|<serial>
param([Parameter(Mandatory)][string]$Serial)
. "$PSScriptRoot\env.ps1"

$Serial = Resolve-GtSerial $Serial
Assert-GtDevice $Serial
$lock = Enter-GtDeviceLock $Serial "glowtide-backkey"
$fail = 0
function Send-Back {
    if ((Get-GtForegroundPackage $Serial) -ne $GT_APP_ID) { throw "GLOWTIDE is not in front: not sending BACK" }
    & $GT_ADB -s $Serial shell input keyevent KEYCODE_BACK | Out-Null
    Start-Sleep -Milliseconds 900
}
try {
    if (-not (Wait-GtDeviceIdle $Serial -QuietSeconds 2 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
    # During a level: BACK pauses, BACK again resumes.
    $appPid = Start-GtApp $Serial '--level 8 --bot greedy'
    Start-Sleep -Seconds 3
    Send-Back
    Send-Back
    $log = @(& $GT_ADB -s $Serial logcat -d --pid=$appPid -s GTLIFE:I)
    foreach ($want in 'screen PLAY -> PAUSE', 'screen PAUSE -> PLAY') {
        if ($log | Select-String ([regex]::Escape($want)) -Quiet) { Write-Host "[back] ok $want" } else { Write-Host "[back] MISSING $want"; ++$fail }
    }
    Stop-GtApp $Serial
    Start-Sleep -Seconds 1
    # On HOME: BACK leaves the app (the process ends).
    $appPid = Start-GtApp $Serial '--fresh'
    Start-Sleep -Seconds 3
    Send-Back
    Start-Sleep -Seconds 2
    if (Get-GtPid $Serial) { Write-Host "[back] MISSING HOME -> leaves the app"; ++$fail; Stop-GtApp $Serial } else { Write-Host "[back] ok HOME -> leaves the app" }
} finally {
    Exit-GtDeviceLock $lock
}
if ($fail) { Write-Host "[back] FAIL ($fail)"; exit 1 }
Write-Host "[back] PASS on $Serial"
exit 0
