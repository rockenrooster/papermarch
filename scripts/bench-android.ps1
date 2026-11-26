# Runs one benchmark on a device and grades it against the contract (PLAN 5, TECH_SPEC 8.3).
#   scripts\bench-android.ps1 -Serial <name|serial> -Scenario bench_flood [-Seconds 60] [-Warmup 180] [-AppArgs '...']
#                             [-Deploy] [-Config dev]
# Takes the shared device lock, waits for the device to be free, launches, waits for the run to end, reads the PMBENCH
# line from logcat (by pid), pulls the full JSON into captures\bench\, prints the grade. An invalid run (focus lost)
# is repeated once.
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Scenario,
    [double]$Seconds = 30,
    [double]$Warmup = 5,
    [string]$AppArgs = '',
    [switch]$Deploy,
    [ValidateSet('dev', 'debug', 'release')][string]$Config = 'dev'
)
. "$PSScriptRoot\env.ps1"

$Serial = Resolve-PmSerial $Serial
Assert-PmDevice $Serial
$id = Get-PmDeviceId $Serial
if ($Deploy) {
    & "$PSScriptRoot\build-android.ps1" -Config $Config -Offline
    & $PM_ADB -s $Serial install -r -t (Join-Path $PM_ROOT "android\app\build\outputs\apk\$Config\app-$Config.apk") | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "install failed on $Serial" }
}
$outDir = Join-Path $PM_ROOT 'captures\bench'
New-Item -ItemType Directory -Force $outDir | Out-Null
$lock = Enter-PmDeviceLock $Serial "papermarch-bench"
try {
    for ($attempt = 1; $attempt -le 2; ++$attempt) {
        if (-not (Wait-PmDeviceIdle $Serial -QuietSeconds 5 -TimeoutSeconds 600)) { throw "$Serial stayed busy" }
        $args2 = "--scenario $Scenario --seconds $Seconds --warmup $Warmup $AppArgs".Trim()
        Write-Host "[bench] $id attempt ${attempt}: $args2"
        $appPid = Start-PmApp $Serial $args2
        if (-not $appPid) { throw "PAPERMARCH did not start on $Serial" }
        $deadline = (Get-Date).AddSeconds($Seconds + $Warmup + 90)
        $foreign = $false
        $homePkg = Get-PmHomePackage $Serial
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Seconds 3
            if (-not (Get-PmPid $Serial)) { break }
            $fg = Get-PmForegroundPackage $Serial
            if ($fg -and $fg -ne $PM_APP_ID -and $fg -ne $homePkg) { $foreign = $true; Write-Host "[bench] $fg came to the front" }
        }
        if (Get-PmPid $Serial) { Stop-PmApp $Serial; throw "the benchmark did not end in time" }
        $log = & $PM_ADB -s $Serial logcat -d --pid=$appPid -s PMBENCH:I PAPERMARCH:I PMLIFE:I
        $line = $log | Select-String 'PMBENCH' | Select-Object -Last 1
        # The sound bank's and the world pads' build times (VARIETY_PLAN V5's exit checks).
        $log | Select-String 'sound bank built|pad built' | ForEach-Object { Write-Host "[bench] $($_.Line -replace '^.*?PAPERMARCH\s*:\s*', '')" }
        if (-not $line) { $log | Select-Object -Last 30 | ForEach-Object { Write-Host $_ }; throw "no PMBENCH line" }
        $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
        $dst = Join-Path $outDir "${id}_${Scenario}_$stamp.json"
        & $PM_ADB -s $Serial pull "/sdcard/Android/data/$PM_APP_ID/files/bench_$Scenario.json" $dst | Out-Null
        if (-not (Test-Path $dst)) { ($line.Line -replace '^.*?PMBENCH\s*:\s*', '') | Set-Content $dst }
        $r = Get-Content $dst -Raw | ConvertFrom-Json
        $valid = $r.valid -and -not $foreign
        Write-Host ("[bench] {0} {1}: refresh {2:N0} Hz, late {3:P2}, worst {4:N1} ms, display p95 {5:N2} ms, cpu p95 {6:N2} ms, tick p95 {7:N3} ms, allocs {8}, wisps {9}, glooms {10}, particles {11}, presents {12} -> {13}{14}" -f `
            $id, $Scenario, $r.refresh_hz, $r.late_share, $r.worst_ms, $r.display_ms[1], $r.cpu_ms[1], $r.tick_ms[1],
            $r.heap_allocs, $r.max_wisps, $r.max_glooms, $r.max_particles, $r.present_timestamps,
            $(if ($r.contract_pass) { 'PASS' } else { 'FAIL' }), $(if ($valid) { '' } else { ' (INVALID: focus lost)' }))
        Write-Host "[bench] $dst"
        if ($valid) { break }
        Write-Host "[bench] invalid run; retrying once in 30 s"
        Start-Sleep -Seconds 30
    }
} finally {
    Stop-PmApp $Serial
    Exit-PmDeviceLock $lock
}
