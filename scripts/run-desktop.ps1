# Builds (unless -NoBuild) and runs the desktop app; returns the app's exit code (TECH_SPEC 2.2, 2.5).
#   scripts\run-desktop.ps1 [-Config debug|dev] [-NoBuild] [-AppArgs '...'] [-Screenshot <png>] [-Frames n]
#                           [-Res WxH] [-Small] [-Sound] [-Watch]
# Silent by default (--silent: everything plays and is measured, the output is zeroed) unless -Sound; off screen
# by default (--offscreen: a hidden window that cannot take focus; rendering goes to a framebuffer object) unless -Watch.
param(
    [ValidateSet('debug', 'dev')][string]$Config = 'debug',
    [switch]$NoBuild,
    [string]$AppArgs = '',
    [string]$Screenshot = '',
    [int]$Frames = 0,
    [string]$Res = '',
    [switch]$Small,
    [switch]$Sound,
    [switch]$Watch
)
. "$PSScriptRoot\env.ps1"

if (-not $NoBuild) { & "$PSScriptRoot\build-desktop.ps1" -Config $Config }
$exe = Join-Path $PM_ROOT "build\desktop-$Config\papermarch.exe"
if (-not (Test-Path $exe)) { throw "missing $exe - build first" }

$argv = @()
if ($AppArgs) { $argv += @($AppArgs -split '\s+' | Where-Object { $_ }) }
if (-not $Sound -and $argv -notcontains '--silent') { $argv += @('--silent') }
if (-not $Watch -and $argv -notcontains '--offscreen') { $argv += @('--offscreen') }
if ($Res) { $argv += @('--res', $Res) }
$shot = $null
if ($Screenshot) {
    $shot = [IO.Path]::GetFullPath($Screenshot, (Get-Location).Path)
    New-Item -ItemType Directory -Force (Split-Path -Parent $shot) | Out-Null
    $argv += @('--screenshot', $shot)
    if ($Frames -le 0) { $Frames = 120 }
}
if ($Frames -gt 0) { $argv += @('--frames', "$Frames") }

Write-Host "[run] $exe $($argv -join ' ')"
& $exe @argv
$code = $LASTEXITCODE
if ($shot -and $Small -and (Test-Path $shot)) { python -I -B (Join-Path $PM_ROOT 'tools\shrink_png.py') $shot }
exit $code
