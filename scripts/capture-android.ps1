# A screenshot of the panel (adb screencap), only while PAPERMARCH is in front (PLAN 3.3).
#   scripts\capture-android.ps1 -Serial <name|serial> [-Out captures\device\x.png] [-Small]
param(
    [Parameter(Mandatory)][string]$Serial,
    [string]$Out = '',
    [switch]$Small
)
. "$PSScriptRoot\env.ps1"
$Serial = Resolve-PmSerial $Serial
Assert-PmDevice $Serial
$fg = Get-PmForegroundPackage $Serial
if ($fg -ne $PM_APP_ID) { throw "PAPERMARCH is not in front on $Serial ($fg): no capture" }
if (-not $Out) { $Out = Join-Path $PM_ROOT "captures\device\$(Get-PmDeviceId $Serial)_$(Get-Date -Format 'yyyyMMdd_HHmmss').png" }
New-Item -ItemType Directory -Force (Split-Path -Parent $Out) | Out-Null
cmd /c "`"$PM_ADB`" -s $Serial exec-out screencap -p > `"$Out`""
Write-Host "[capture] $Out"
if ($Small) { python -I -B (Join-Path $PM_ROOT 'tools\shrink_png.py') $Out }
