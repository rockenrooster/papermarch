# Background/foreground cycles (P0, P3): HOME, then back to GLOWTIDE, N times; then checks the app is alive and drawing.
# Sends HOME only while the launcher or GLOWTIDE is in front (PLAN 3.3); skips (and says so) when the device is busy.
#   scripts\lifecycle-android.ps1 -Serial <name|serial> [-Cycles 20] [-AppArgs '...']
param(
    [Parameter(Mandatory)][string]$Serial,
    [int]$Cycles = 20,
    [string]$AppArgs = ''
)
. "$PSScriptRoot\env.ps1"
$Serial = Resolve-GtSerial $Serial
Assert-GtDevice $Serial
$lock = Enter-GtDeviceLock $Serial 'glowtide-lifecycle'
try {
    if (-not (Wait-GtDeviceIdle $Serial -QuietSeconds 3 -TimeoutSeconds 120)) { Write-Host "[lifecycle] $Serial busy: not verified"; exit 3 }
    Assert-GtUnlocked $Serial
    $appPid = Start-GtApp $Serial $AppArgs
    # The app's lines are streamed to a file as they are written: a small log buffer (the 12R's holds about a minute)
    # would have dropped the early cycles' lines by the end of the run.
    $logFile = Join-Path ([IO.Path]::GetTempPath()) "glowtide_lifecycle_$PID.txt"
    $logcat = Start-Process -FilePath $GT_ADB -ArgumentList @('-s', $Serial, 'logcat', '--pid', $appPid, '-s', 'GTLIFE:I', 'GLOWTIDE:E') `
        -RedirectStandardOutput $logFile -NoNewWindow -PassThru
    Start-Sleep -Seconds 3
    for ($i = 1; $i -le $Cycles; ++$i) {
        if (-not (Test-GtDeviceFree $Serial)) { Write-Host "[lifecycle] another app came to the front: stopping"; exit 3 }
        & $GT_ADB -s $Serial shell input keyevent KEYCODE_HOME | Out-Null
        Start-Sleep -Milliseconds 1200
        & $GT_ADB -s $Serial shell "am start -n $GT_ACTIVITY" | Out-Null
        Start-Sleep -Milliseconds 1500
        $now = Get-GtPid $Serial
        if ($now -ne $appPid) { Write-Host "[lifecycle] cycle ${i}: the process changed ($appPid -> '$now'): crash?"; exit 1 }
    }
    Start-Sleep -Seconds 2
    Stop-Process -Id $logcat.Id -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 300
    $log = @(Get-Content $logFile -ErrorAction SilentlyContinue)
    Remove-Item $logFile -ErrorAction SilentlyContinue
    $fronts = @($log | Select-String 'first frame after resume').Count
    $lost = @($log | Select-String 'context was lost').Count
    $errors = @($log | Select-String ' E GLOWTIDE').Count
    $paused = @($log | Select-String 'resumed on PAUSE').Count
    Write-Host "[lifecycle] $Cycles cycles: $fronts first frames after resume, $paused resumed on PAUSE, $lost context rebuilds, $errors errors"
    Stop-GtApp $Serial
    if ($fronts -lt $Cycles -or $errors -gt 0) { exit 1 }
} finally {
    Exit-GtDeviceLock $lock
}
