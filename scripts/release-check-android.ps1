# The release APK on a device (P6 exit check): it installs, starts, ignores app arguments and is not debuggable. The
# dev build is put back afterwards (it has the level stepper the owner's playtest uses).
#   scripts\release-check-android.ps1 -Serial moto
param([Parameter(Mandatory)][string]$Serial)
. "$PSScriptRoot\env.ps1"
# The release build has no id suffix (android/app/build.gradle gives GT_APP_ID_SUFFIX to dev builds only): the checks
# drive that package. Installing it replaces only a build with the same key (a local one); a GitHub copy refuses it.
$GT_APP_ID = 'dev.glowtide.game'
$GT_ACTIVITY = "$GT_APP_ID/dev.glowtide.game.GlowtideActivity"

$Serial = Resolve-GtSerial $Serial
Assert-GtDevice $Serial
$release = Join-Path $GT_ROOT 'android\app\build\outputs\apk\release\app-release.apk'
$dev = Join-Path $GT_ROOT 'android\app\build\outputs\apk\dev\app-dev.apk'
$lock = Enter-GtDeviceLock $Serial "glowtide-release"
$fail = 0
try {
    if (-not (Wait-GtDeviceIdle $Serial -QuietSeconds 2 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
    Stop-GtApp $Serial
    & $GT_ADB -s $Serial install -r $release | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "install of the release APK failed" }
    # Arguments that would quit after five seconds in a debuggable build.
    $appPid = Start-GtApp $Serial '--scenario bench_idle --seconds 5 --warmup 0'
    Start-Sleep -Seconds 9
    $alive = [bool](Get-GtPid $Serial)
    $log = @(& $GT_ADB -s $Serial logcat -d --pid=$appPid -s GLOWTIDE:I)
    $scenario = $log | Select-String 'scenario (\S+)' | Select-Object -First 1
    $runAs = "$(& $GT_ADB -s $Serial shell "run-as $GT_APP_ID ls" 2>&1)"
    if ($alive -and $scenario -match 'scenario play') { Write-Host "[release] ok: starts and ignores its arguments (still running, scenario play)" }
    else { Write-Host "[release] FAIL: alive $alive, '$scenario'"; ++$fail }
    if ($runAs -match 'not debuggable') { Write-Host "[release] ok: not debuggable ($($runAs.Trim()))" }
    else { Write-Host "[release] FAIL: run-as answered '$runAs'"; ++$fail }
    Stop-GtApp $Serial
} finally {
    & $GT_ADB -s $Serial install -r -t $dev | Out-Null
    Write-Host "[release] the dev build is back on $Serial"
    Exit-GtDeviceLock $lock
}
if ($fail) { exit 1 }
Write-Host "[release] PASS on $Serial"
exit 0
