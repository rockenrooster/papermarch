# Picture sets with contact sheets (TECH_SPEC 9.1): renders scripted pictures on the PC (silent, off screen) and tiles them.
#   scripts\review.ps1 -Set p1|screens|levels|worlds|strips [-Config dev] [-NoBuild] [-From N] [-Count 30] [-Levels 12,40] [-Lint]
# p1: the concept frame at the eight frames of the matrix. screens: every screen. levels: thumbnails of consecutive levels at
# their start (or -Levels). worlds: the first level of each world. strips: a move's clips, frame by frame.
param(
    [ValidateSet('p1', 'screens', 'levels', 'worlds', 'strips')][string]$Set = 'p1',
    [ValidateSet('debug', 'dev')][string]$Config = 'dev',
    [switch]$NoBuild,
    [int]$From = 1,
    [int]$Count = 30,
    [int[]]$Levels = @(),
    [switch]$Lint
)
. "$PSScriptRoot\env.ps1"
if (-not $NoBuild) { & "$PSScriptRoot\build-desktop.ps1" -Config $Config }
$exe = Join-Path $PM_ROOT "build\desktop-$Config\papermarch.exe"
$out = Join-Path $PM_ROOT "captures\review\$Set"
New-Item -ItemType Directory -Force $out | Out-Null
$failed = 0
$shots = @()

function Shot([string]$Name, [string[]]$AppArgs, [string]$Res = '1080x2400', [int]$Frames = 150) {
    $png = Join-Path $out "$Name.png"
    $a = @($AppArgs) + @('--res', $Res, '--frames', "$Frames", '--screenshot', $png, '--silent', '--offscreen', '--no-audio')
    if ($Lint) { $a += '--lint' }
    & $exe @a 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "[review] $Name failed ($LASTEXITCODE)"; $script:failed++ }
    $script:shots += $png
}

switch ($Set) {
    'p1' {
        foreach ($res in '1080x2400', '540x1200', '1080x2640', '1080x1920', '600x1024', '1200x1920', '1080x1440', '1264x2780') {
            Shot "concept_$res" @('--picture', 'concept') $res 240
        }
    }
    'screens' {
        foreach ($s in 'screen_play', 'screen_pause', 'screen_win', 'screen_lose', 'screen_settings', 'screen_levels', 'screen_end', 'home_update') {
            Shot $s @('--picture', $s) '1080x2400' 420
        }
    }
    'levels' {
        $nums = if ($Levels.Count) { $Levels } else { $From..($From + $Count - 1) }
        foreach ($n in $nums) { Shot ("level_{0:D3}" -f $n) @('--picture', 'level', '--level', "$n") '1080x2400' 90 }
    }
    'worlds' {
        foreach ($w in 1..10) { Shot ("world_{0:D2}" -f $w) @('--picture', 'world', '--world', "$w") '1080x2400' 90 }
    }
    'strips' {
        # A camp taken on level 12: the march, the fight, the flip and the count, every sixth frame.
        Shot 'battle' @('--picture', 'level', '--level', '12', '--moves', '1', '--strip', '12', '--strip-step', '6') '540x1200' 80
    }
}
$cols = if ($Set -eq 'levels') { 10 } else { [math]::Min(8, $shots.Count) }
if ($Set -eq 'strips') {
    python -I -B (Join-Path $PM_ROOT 'tools\contact.py') strip (Join-Path $out 'battle_strip.png') (Join-Path $out 'battle.png')
} elseif ($shots.Count) {
    python -I -B (Join-Path $PM_ROOT 'tools\contact.py') sheet (Join-Path $out "$Set.png") @shots --cols $cols --height 700
}
if ($failed) { Write-Host "[review] $failed failed"; exit 1 }
exit 0
