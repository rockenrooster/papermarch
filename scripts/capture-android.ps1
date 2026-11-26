# A screenshot of the panel (adb screencap), only while GLOWTIDE is in front (PLAN 3.3).
#   scripts\capture-android.ps1 -Serial <name|serial> [-Out captures\device\x.png] [-Small]
param(
    [Parameter(Mandatory)][string]$Serial,
    [string]$Out = '',
    [switch]$Small
)
. "$PSScriptRoot\env.ps1"
$Serial = Resolve-GtSerial $Serial
Assert-GtDevice $Serial
$fg = Get-GtForegroundPackage $Serial
if ($fg -ne $GT_APP_ID) { throw "GLOWTIDE is not in front on $Serial ($fg): no capture" }
if (-not $Out) { $Out = Join-Path $GT_ROOT "captures\device\$(Get-GtDeviceId $Serial)_$(Get-Date -Format 'yyyyMMdd_HHmmss').png" }
New-Item -ItemType Directory -Force (Split-Path -Parent $Out) | Out-Null
cmd /c "`"$GT_ADB`" -s $Serial exec-out screencap -p > `"$Out`""
Write-Host "[capture] $Out"
if ($Small) { python -I -B (Join-Path $GT_ROOT 'tools\shrink_png.py') $Out }
