# Runs the flow test (TECH_SPEC 8.4) on one device: lock, install (unless -NoInstall), launch with --flow-test, wait for
# the app to end, print the FLOW lines. Exit 0 when every check passed.
#   scripts\flow-android.ps1 -Serial moto|tablet|op12r|<serial> [-NoInstall] [-TimeoutSeconds 240]
param(
    [Parameter(Mandatory)][string]$Serial,
    [switch]$NoInstall,
    [int]$TimeoutSeconds = 240
)
. "$PSScriptRoot\env.ps1"

$Serial = Resolve-PmSerial $Serial
Assert-PmDevice $Serial
$lock = Enter-PmDeviceLock $Serial "papermarch-flow"
try {
    if (-not $NoInstall) {
        $apk = if ($env:PM_APK) { $env:PM_APK } else { Join-Path $PM_ROOT 'android\app\build\outputs\apk\dev\app-dev.apk' }
        & $PM_ADB -s $Serial install -r -t $apk | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "install failed on $Serial" }
    }
    if (-not (Wait-PmDeviceIdle $Serial -QuietSeconds 2 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
    $appPid = Start-PmApp $Serial '--flow-test'
    if (-not $appPid) { throw "PAPERMARCH did not start on $Serial" }
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline -and (Get-PmPid $Serial)) { Start-Sleep -Seconds 2 }
    if (Get-PmPid $Serial) { Stop-PmApp $Serial; throw "the flow test did not end in $TimeoutSeconds s" }
    $log = @(& $PM_ADB -s $Serial logcat -d --pid=$appPid -s FLOW:I PAPERMARCH:I)
    $log | Where-Object { $_ -match 'FLOW|exit|audio:' } | ForEach-Object { Write-Host $_ }
    $done = $log | Where-Object { $_ -match 'done: (\d+) checks, (\d+) failed' } | Select-Object -Last 1
    if (-not $done) { Write-Host "[flow] no result line"; exit 1 }
    $null = $done -match 'done: (\d+) checks, (\d+) failed'
    if ([int]$Matches[2] -gt 0) { Write-Host "[flow] FAIL on $Serial"; exit 6 }
    Write-Host "[flow] PASS on $Serial ($($Matches[1]) checks)"
} finally {
    Exit-PmDeviceLock $lock
}
exit 0
