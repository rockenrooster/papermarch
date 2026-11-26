# Heat soak (P6 task 4): bench_play looping on the heaviest level for twenty minutes; the last 60 s are measured
# against the contract; battery temperature and the app's memory (PSS) are logged every minute.
#   scripts\soak-android.ps1 -Serial moto [-Minutes 20] [-Level 87]
param(
    [Parameter(Mandatory)][string]$Serial,
    [int]$Minutes = 20,
    [int]$Level = 87
)
. "$PSScriptRoot\env.ps1"

$Serial = Resolve-GtSerial $Serial
Assert-GtDevice $Serial
$id = Get-GtDeviceId $Serial
$outDir = Join-Path $GT_ROOT 'captures\soak'
New-Item -ItemType Directory -Force $outDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$csv = Join-Path $outDir "${id}_soak_$stamp.csv"
"minute,battery_c,pss_kb,foreground" | Set-Content $csv
$lock = Enter-GtDeviceLock $Serial "glowtide-soak"
$rc = 0
try {
    if (-not (Wait-GtDeviceIdle $Serial -QuietSeconds 3 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
    $warm = $Minutes * 60 - 60
    & $GT_ADB -s $Serial shell "rm -f /sdcard/Android/data/$GT_APP_ID/files/bench_bench_play.json" | Out-Null  # no stale result
    $appPid = Start-GtApp $Serial "--scenario bench_play --level $Level --seconds 60 --warmup $warm"
    if (-not $appPid) { throw "GLOWTIDE did not start" }
    $t0 = Get-Date
    # One sample on each minute mark (the last one 30 s before the run ends); memory is the median of three reads
    # 3 s apart, so one level reload's transient GL buffers do not count as growth.
    function Get-Pss {
        $p = ((& $GT_ADB -s $Serial shell dumpsys meminfo $appPid) | Select-String 'TOTAL PSS:\s*(\d+)|^\s*TOTAL\s+(\d+)' | Select-Object -First 1)
        if (-not $p) { return $null }
        $g = $p.Matches[0].Groups
        return [int]$(if ($g[1].Value) { $g[1].Value } else { $g[2].Value })
    }
    for ($m = 1; $m -le $Minutes; ++$m) {
        $at = [math]::Min($m * 60, $Minutes * 60 - 30)
        while (((Get-Date) - $t0).TotalSeconds -lt $at) {
            if (-not (Get-GtPid $Serial)) { break }
            Start-Sleep -Seconds 2
        }
        if (-not (Get-GtPid $Serial)) { break }
        $temp = ((& $GT_ADB -s $Serial shell dumpsys battery) | Select-String 'temperature:\s*(\d+)').Matches.Groups[1].Value
        $reads = @()
        for ($k = 0; $k -lt 3; ++$k) {
            $v = Get-Pss
            if ($v) { $reads += $v }
            if ($k -lt 2) { Start-Sleep -Seconds 3 }
        }
        $kb = if ($reads.Count) { ($reads | Sort-Object)[[int][math]::Floor(($reads.Count - 1) / 2)] } else { '' }
        $fg = Get-GtForegroundPackage $Serial
        $c = if ($temp) { '{0:N1}' -f ([int]$temp / 10.0) } else { '' }
        "$m,$c,$kb,$fg" | Add-Content $csv
        Write-Host "[soak] minute ${m}: battery $c C, PSS $kb kB, front $fg"
    }
    $deadline = (Get-Date).AddMinutes(3)
    while ((Get-GtPid $Serial) -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 5 }
    $json = Join-Path $outDir "${id}_soak_$stamp.json"
    & $GT_ADB -s $Serial pull "/sdcard/Android/data/$GT_APP_ID/files/bench_bench_play.json" $json | Out-Null
    $b = Get-Content $json -Raw | ConvertFrom-Json
    $rows = Import-Csv $csv | Where-Object { $_.pss_kb }
    $memDrift = if ($rows.Count -ge 2) { 100.0 * ([double]$rows[-1].pss_kb - [double]$rows[0].pss_kb) / [double]$rows[0].pss_kb } else { 0 }
    $pass = $b.contract_pass -and $b.valid -and [math]::Abs($memDrift) -le 5.0
    Write-Host ("[soak] last 60 s: late {0:P2}, worst {1:N1} ms, cpu p95 {2:N2} ms, valid {3}; memory drift {4:N1}% -> {5}" -f
        $b.late_share, $b.worst_ms, $b.cpu_ms[1], $b.valid, $memDrift, $(if ($pass) { 'PASS' } else { 'FAIL' }))
    Write-Host "[soak] $csv"
    if (-not $pass) { $rc = 1 }
} finally {
    Exit-GtDeviceLock $lock
}
exit $rc
