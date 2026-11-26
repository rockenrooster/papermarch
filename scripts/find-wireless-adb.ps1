# Finds a device's wireless-debugging port without mDNS and connects adb to it (P6: the tablet and the 12R).
#   scripts\find-wireless-adb.ps1 -Ip <address>[,<address>]
# The addresses: the phones' own on the local network (Settings, About, Status; or RAVEFORM's co-op logs). The device
# must have Wireless debugging switched on and must already trust this PC (paired before); a port that is not adbd is
# skipped. Prints the serial of every device it connected.
param([Parameter(Mandatory)][string[]]$Ip)
. "$PSScriptRoot\env.ps1"

$Ip = @($Ip | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$hits = @(python -I -B (Join-Path $GT_ROOT 'tools\find_adb_port.py') @Ip)
if (-not $hits.Count) { Write-Host "[find] no open port in 30000-61000 on $($Ip -join ', '): Wireless debugging is off (or the device is away)"; exit 1 }
$connected = @()
foreach ($h in $hits) {
    $known = @(& $GT_ADB devices | Select-String "^$([regex]::Escape($h))\s+device$").Count -gt 0
    if (-not $known) { & $GT_ADB connect $h 2>&1 | Out-Null; Start-Sleep -Milliseconds 800 }
    if (@(& $GT_ADB devices | Select-String "^$([regex]::Escape($h))\s+device$").Count) {
        $serial = "$(& $GT_ADB -s $h shell getprop ro.serialno)".Trim()
        $model = "$(& $GT_ADB -s $h shell getprop ro.product.model)".Trim()
        Write-Host "[find] $h is $model ($serial)"
        $connected += $h
    } else {
        & $GT_ADB disconnect $h 2>&1 | Out-Null  # not adbd, or not trusting this PC: leave no offline transport
    }
}
if (-not $connected.Count) { Write-Host "[find] open ports $($hits -join ', '), none of them a debuggable device that trusts this PC"; exit 1 }
exit 0
