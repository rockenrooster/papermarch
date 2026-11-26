# The device picture check (VARIETY_TECH 10 and 12, trap 1): a phone renders a picture, the PNG is pulled, the PC renders
# the same picture at the phone's frame size, dp scale and safe insets, and tools\picture_compare.py compares the two.
# A phone once passed every other check while drawing no crowd at all (the OnePlus 12R, 2026-11-20): this catches that.
#   scripts\picture-android.ps1 -Serial moto|tablet|op12r|<serial> [-Picture concept] [-Against <picture>] [-Install]
#                               [-MaxDiff 4] [-Config dev]
# -Against renders another picture on the PC (to see the check fail). Exit 0 when the pictures agree.
param(
    [Parameter(Mandatory)][string]$Serial,
    [string]$Picture = 'concept',
    [string]$Against = '',
    [switch]$Install,
    [double]$MaxDiff = 4,  # good runs: 0.1 (tablet); a different picture: 11.6
    [ValidateSet('debug', 'dev')][string]$Config = 'dev',
    [int]$Frames = 120,
    [int]$TimeoutSeconds = 90
)
. "$PSScriptRoot\env.ps1"
if (-not $Against) { $Against = $Picture }

$Serial = Resolve-PmSerial $Serial
Assert-PmDevice $Serial
$id = Get-PmDeviceId $Serial
$out = Join-Path $PM_ROOT 'captures\pictures'
New-Item -ItemType Directory -Force $out | Out-Null
$devPng = Join-Path $out "${Picture}_$id.png"
$pcPng = Join-Path $out "${Against}_pc_as_$id.png"
Remove-Item $devPng, $pcPng -ErrorAction SilentlyContinue
$name = "pic_$Picture.png"
$frameLine = $null
$lock = Enter-PmDeviceLock $Serial 'papermarch-picture'
$awake = $null
try {
    if ($Install) {
        $apk = if ($env:PM_APK) { $env:PM_APK } else { Join-Path $PM_ROOT 'android\app\build\outputs\apk\dev\app-dev.apk' }
        & $PM_ADB -s $Serial install -r -t $apk | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "install failed on $Serial" }
    }
    if (-not (Wait-PmDeviceIdle $Serial -QuietSeconds 2 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
    Assert-PmUnlocked $Serial
    $awake = Start-PmKeepAwake $Serial
    $appPid = Start-PmApp $Serial "--picture $Picture --frames $Frames --screenshot $name"
    if (-not $appPid) { throw "PAPERMARCH did not start on $Serial" }
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $held = $false
    while ((Get-Date) -lt $deadline) {
        $log = @(& $PM_ADB -s $Serial logcat -d --pid=$appPid -s PAPERMARCH:I)
        # The last frame line: the first can come before the window's insets are known.
        $fl = $log | Where-Object { $_ -match 'frame \d+x\d+ px, dp scale' } | Select-Object -Last 1
        if ($fl) { $frameLine = $fl }
        if ($log | Where-Object { $_ -match 'picture: holding frame' }) { $held = $true; break }
        if (-not (Get-PmPid $Serial)) { break }
        Start-Sleep -Seconds 1
    }
    if (-not $held) { throw "the picture was not held on $Serial within $TimeoutSeconds s" }
    & $PM_ADB -s $Serial pull "/sdcard/Android/data/$PM_APP_ID/files/$name" $devPng | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $devPng)) { throw "could not pull $name from $Serial" }
    Stop-PmApp $Serial
} finally {
    Stop-PmKeepAwake $awake
    Exit-PmDeviceLock $lock
}
if ("$frameLine" -notmatch 'frame (\d+)x(\d+) px, dp scale ([\d.]+), safe top ([\d.]+) bottom ([\d.]+) px') {
    Write-Host "[picture] no frame line in the app's log"; exit 1
}
$w, $h, $dp, $top, $bottom = $Matches[1], $Matches[2], [double]$Matches[3], [double]$Matches[4], [double]$Matches[5]
$safe = '{0:0.###},{1:0.###}' -f ($top / $dp), ($bottom / $dp)
$pcBuild = if ($env:PM_PC_BUILD) { $env:PM_PC_BUILD } else { Join-Path $PM_ROOT "build\desktop-$Config" }  # see simhash-android.ps1
$exe = Join-Path $pcBuild 'papermarch.exe'
& $exe --picture $Against --silent --offscreen --res "${w}x$h" --dp-scale $dp --safe $safe --screenshot $pcPng --frames $Frames | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host "[picture] the PC render failed ($LASTEXITCODE)"; exit 1 }
Write-Host "[picture] $Picture on $id (${w}x$h, dp $dp, safe $safe dp) against the PC's $Against"
python -I -B (Join-Path $PM_ROOT 'tools\picture_compare.py') $devPng $pcPng --max-diff $MaxDiff
$code = $LASTEXITCODE
Write-Host ("[picture] {0} on {1}" -f $(if ($code -eq 0) { 'PASS' } else { 'FAIL' }), $id)
exit $code
