# Runs one benchmark on a device and grades it against the contract (PLAN 5, TECH_SPEC 8.3).
#   scripts\bench-android.ps1 -Serial <name|serial> -Scenario bench_flood [-Seconds 60] [-Warmup 180] [-AppArgs '...']
#                             [-Deploy] [-Config dev]
# Takes the shared device lock, waits for the device to be free, launches, waits for the run to end, reads the GTBENCH
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

$Serial = Resolve-GtSerial $Serial
Assert-GtDevice $Serial
$id = Get-GtDeviceId $Serial
if ($Deploy) {
    & "$PSScriptRoot\build-android.ps1" -Config $Config -Offline
    & $GT_ADB -s $Serial install -r -t (Join-Path $GT_ROOT "android\app\build\outputs\apk\$Config\app-$Config.apk") | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "install failed on $Serial" }
}
$outDir = Join-Path $GT_ROOT 'captures\bench'
New-Item -ItemType Directory -Force $outDir | Out-Null
$lock = Enter-GtDeviceLock $Serial "glowtide-bench"
try {
    for ($attempt = 1; $attempt -le 2; ++$attempt) {
        if (-not (Wait-GtDeviceIdle $Serial -QuietSeconds 5 -TimeoutSeconds 600)) { throw "$Serial stayed busy" }
        $args2 = "--scenario $Scenario --seconds $Seconds --warmup $Warmup $AppArgs".Trim()
        Write-Host "[bench] $id attempt ${attempt}: $args2"
        $appPid = Start-GtApp $Serial $args2
        if (-not $appPid) { throw "GLOWTIDE did not start on $Serial" }
        $deadline = (Get-Date).AddSeconds($Seconds + $Warmup + 90)
        $foreign = $false
        $homePkg = Get-GtHomePackage $Serial
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Seconds 3
            if (-not (Get-GtPid $Serial)) { break }
            $fg = Get-GtForegroundPackage $Serial
            if ($fg -and $fg -ne $GT_APP_ID -and $fg -ne $homePkg) { $foreign = $true; Write-Host "[bench] $fg came to the front" }
        }
        if (Get-GtPid $Serial) { Stop-GtApp $Serial; throw "the benchmark did not end in time" }
        $log = & $GT_ADB -s $Serial logcat -d --pid=$appPid -s GTBENCH:I GLOWTIDE:I GTLIFE:I
        $line = $log | Select-String 'GTBENCH' | Select-Object -Last 1
        # The sound bank's and the world pads' build times (VARIETY_PLAN V5's exit checks).
        $log | Select-String 'sound bank built|pad built' | ForEach-Object { Write-Host "[bench] $($_.Line -replace '^.*?GLOWTIDE\s*:\s*', '')" }
        if (-not $line) { $log | Select-Object -Last 30 | ForEach-Object { Write-Host $_ }; throw "no GTBENCH line" }
        $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
        $dst = Join-Path $outDir "${id}_${Scenario}_$stamp.json"
        & $GT_ADB -s $Serial pull "/sdcard/Android/data/$GT_APP_ID/files/bench_$Scenario.json" $dst | Out-Null
        if (-not (Test-Path $dst)) { ($line.Line -replace '^.*?GTBENCH\s*:\s*', '') | Set-Content $dst }
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
    Stop-GtApp $Serial
    Exit-GtDeviceLock $lock
}
