# The system back key (GAME_DESIGN 4): play -> pause -> play, and on HOME it leaves the app. Sends BACK only while
# PAPERMARCH is in front (PLAN 3.3) and reads the screen changes the game logs (PMLIFE).
#   scripts\backkey-android.ps1 -Serial moto|tablet|op12r|<serial>
param([Parameter(Mandatory)][string]$Serial)
. "$PSScriptRoot\env.ps1"

$Serial = Resolve-PmSerial $Serial
Assert-PmDevice $Serial
$lock = Enter-PmDeviceLock $Serial "papermarch-backkey"
$fail = 0
function Send-Back {
    if ((Get-PmForegroundPackage $Serial) -ne $PM_APP_ID) { throw "PAPERMARCH is not in front: not sending BACK" }
    & $PM_ADB -s $Serial shell input keyevent KEYCODE_BACK | Out-Null
    Start-Sleep -Milliseconds 900
}
try {
    if (-not (Wait-PmDeviceIdle $Serial -QuietSeconds 2 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
    # During a level: BACK pauses, BACK again resumes.
    $appPid = Start-PmApp $Serial '--level 8 --bot greedy'
    Start-Sleep -Seconds 3
    Send-Back
    Send-Back
    $log = @(& $PM_ADB -s $Serial logcat -d --pid=$appPid -s PMLIFE:I)
    foreach ($want in 'screen PLAY -> PAUSE', 'screen PAUSE -> PLAY') {
        if ($log | Select-String ([regex]::Escape($want)) -Quiet) { Write-Host "[back] ok $want" } else { Write-Host "[back] MISSING $want"; ++$fail }
    }
    Stop-PmApp $Serial
    Start-Sleep -Seconds 1
    # On HOME: BACK leaves the app (the process ends).
    $appPid = Start-PmApp $Serial '--fresh'
    Start-Sleep -Seconds 3
    Send-Back
    Start-Sleep -Seconds 2
    if (Get-PmPid $Serial) { Write-Host "[back] MISSING HOME -> leaves the app"; ++$fail; Stop-PmApp $Serial } else { Write-Host "[back] ok HOME -> leaves the app" }
} finally {
    Exit-PmDeviceLock $lock
}
if ($fail) { Write-Host "[back] FAIL ($fail)"; exit 1 }
Write-Host "[back] PASS on $Serial"
exit 0
