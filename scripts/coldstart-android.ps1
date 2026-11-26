# Cold start (P3 exit check): force-stop, launch with `am start -W`, read the time from the launch request
# (ActivityTaskManager START) to our first swapped frame (GTLIFE) from logcat timestamps. Repeats N times.
#   scripts\coldstart-android.ps1 -Serial moto [-Runs 5]
param(
    [Parameter(Mandatory)][string]$Serial,
    [int]$Runs = 5
)
. "$PSScriptRoot\env.ps1"

$Serial = Resolve-GtSerial $Serial
Assert-GtDevice $Serial
$lock = Enter-GtDeviceLock $Serial "glowtide-coldstart"
$results = @()
try {
    for ($i = 1; $i -le $Runs; ++$i) {
        if (-not (Wait-GtDeviceIdle $Serial -QuietSeconds 2 -TimeoutSeconds 120)) { throw "$Serial stayed busy" }
        Stop-GtApp $Serial
        Invoke-GtWake $Serial
        Start-Sleep -Seconds 2
        $since = (& $GT_ADB -s $Serial shell 'date +%m-%d\ %H:%M:%S.000').Trim()
        $out = & $GT_ADB -s $Serial shell "am start -W -n $GT_ACTIVITY"
        $total = ($out | Select-String 'TotalTime: (\d+)').Matches.Groups[1].Value
        Start-Sleep -Seconds 3
        $log = @(& $GT_ADB -s $Serial logcat -d -v time -T "$since" -s ActivityTaskManager:I GTLIFE:I)
        $start = $log | Where-Object { $_ -match "START u0.*$([regex]::Escape($GT_APP_ID))" } | Select-Object -First 1
        $first = $log | Where-Object { $_ -match 'first frame swapped' } | Select-Object -First 1
        if (-not $start -or -not $first) { Write-Host "[coldstart] run ${i}: log lines missing"; continue }
        $t0 = [datetime]::ParseExact($start.Substring(0, 18), 'MM-dd HH:mm:ss.fff', $null)
        $t1 = [datetime]::ParseExact($first.Substring(0, 18), 'MM-dd HH:mm:ss.fff', $null)
        $ms = [int]($t1 - $t0).TotalMilliseconds
        $results += $ms
        Write-Host "[coldstart] run ${i}: am start TotalTime $total ms, launch request to first frame $ms ms"
    }
    Stop-GtApp $Serial
} finally {
    Exit-GtDeviceLock $lock
}
if (-not $results.Count) { exit 1 }
$sorted = $results | Sort-Object
$median = $sorted[[int][math]::Floor(($sorted.Count - 1) / 2)]
Write-Host "[coldstart] median $median ms, worst $($sorted[-1]) ms (target 2000 ms)"
if ($median -gt 2000) { exit 1 }
exit 0
