# Builds (unless -NoBuild), installs and launches PAPERMARCH on one device, with the shared-device checks (PLAN 3.3).
#   scripts\deploy-android.ps1 -Serial <name|serial> [-Config dev] [-NoBuild] [-NoLaunch] [-AppArgs '...']
param(
    [Parameter(Mandatory)][string]$Serial,
    [ValidateSet('dev', 'debug', 'release')][string]$Config = 'dev',
    [switch]$NoBuild,
    [switch]$NoLaunch,
    [string]$AppArgs = ''
)
. "$PSScriptRoot\env.ps1"

if (-not $NoBuild) { & "$PSScriptRoot\build-android.ps1" -Config $Config -Offline }
$Serial = Resolve-PmSerial $Serial
Assert-PmDevice $Serial
$apk = if ($env:PM_APK) { $env:PM_APK } else { Join-Path $PM_ROOT "android\app\build\outputs\apk\$Config\app-$Config.apk" }  # PM_APK: a frozen APK
& $PM_ADB -s $Serial install -r -t $apk
if ($LASTEXITCODE -ne 0) { throw "install failed on $Serial" }
if (-not $NoLaunch) {
    if (-not (Wait-PmDeviceIdle $Serial -QuietSeconds 2 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
    $appPid = Start-PmApp $Serial $AppArgs
    Write-Host "[deploy] $PM_APP_ID running on $Serial (pid $appPid)"
}
