# Dot-source this from every script. It sets the per-process environment for PAPERMARCH builds and device runs.
# It never changes system or user settings (docs/PLAN.md 3.1 lists why each override exists). Adapted from RAVEFORM's.

$ErrorActionPreference = 'Stop'

$PM_ROOT     = Split-Path -Parent $PSScriptRoot
$PM_JDK      = 'C:\Program Files\Eclipse Adoptium\jdk-17.0.21.9-hotspot'   # java on PATH is an old shim
$PM_VKSDK    = 'C:\VulkanSDK\1.4.363.0'                                    # only for glslangValidator
$PM_CMAKE    = 'C:\Program Files\CMake\bin\cmake.exe'                      # PATH cmake is an MSYS2 build
$PM_GIT      = 'C:\Program Files\Git\cmd\git.exe'                          # PATH git is an MSYS2 build
$PM_SDK      = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
$PM_NDK      = Join-Path $PM_SDK 'ndk\29.0.14206865'
$PM_ADB      = Join-Path $PM_SDK 'platform-tools\adb.exe'
# PM_APP_ID_SUFFIX (e.g. .dev, as android/app/build.gradle reads it): the device scripts drive a dev build installed beside
# a GitHub build (another signature) instead of replacing it.
$PM_APP_ID   = 'dev.papermarch.game' + $env:PM_APP_ID_SUFFIX
$PM_ACTIVITY = "$PM_APP_ID/dev.papermarch.game.PapermarchActivity"
$PM_LOCKS    = 'D:\Stuff\DevFolder\Android_Games\Raveform\captures\locks'  # shared with RAVEFORM's sessions (PLAN 3.3)

foreach ($p in @($PM_JDK, $PM_CMAKE, $PM_GIT, $PM_NDK, $PM_ADB)) {
    if (-not (Test-Path $p)) { throw "Missing tool: $p (see docs/PLAN.md section 3.1)" }
}

# --- CPU limit (owner's rule: anything this project runs on the PC uses at most 4 CPU cores) ----------------
# This PowerShell process is confined to one logical CPU on each of 4 physical cores of one L3 domain; every
# process it starts inherits that. Builds also limit their own jobs ($PM_JOBS). Per process only.
$PM_JOBS = 4
if (-not ('PmCpuTopology' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class PmCpuTopology {
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetLogicalProcessorInformation(IntPtr buffer, ref uint length);
    public static void Read(List<ulong> cores, List<ulong> l3) {
        uint len = 0;
        GetLogicalProcessorInformation(IntPtr.Zero, ref len);
        if (len == 0) return;
        IntPtr buf = Marshal.AllocHGlobal((int)len);
        try {
            if (!GetLogicalProcessorInformation(buf, ref len)) return;
            int size = IntPtr.Size == 8 ? 32 : 24;
            int unionOffset = IntPtr.Size == 8 ? 16 : 8;
            for (int off = 0; off + size <= (int)len; off += size) {
                ulong mask = (ulong)Marshal.ReadIntPtr(buf, off).ToInt64();
                int relation = Marshal.ReadInt32(buf, off + IntPtr.Size);
                if (relation == 0) cores.Add(mask);
                else if (relation == 2 && Marshal.ReadByte(buf, off + unionOffset) == 3) l3.Add(mask);
            }
        } finally { Marshal.FreeHGlobal(buf); }
    }
}
'@
}

function Get-PmCpuMask([int]$Cores = $PM_JOBS) {
    $coreMasks = [System.Collections.Generic.List[uint64]]::new()
    $l3Masks = [System.Collections.Generic.List[uint64]]::new()
    [PmCpuTopology]::Read($coreMasks, $l3Masks)
    if ($coreMasks.Count -eq 0) { return [uint64]0 }
    $domains = if ($l3Masks.Count) { @($l3Masks) } else { @([uint64]::MaxValue) }
    for ($d = $domains.Count - 1; $d -ge 0; --$d) {
        $inDomain = @($coreMasks | Where-Object { ($_ -band $domains[$d]) -eq $_ })
        if ($inDomain.Count -lt $Cores) { continue }
        [uint64]$mask = 0
        foreach ($core in ($inDomain | Select-Object -Last $Cores)) {
            for ($bit = 0; $bit -lt 64; ++$bit) {
                if (($core -shr $bit) -band 1) { $mask = $mask -bor ([uint64]1 -shl $bit); break }
            }
        }
        return $mask
    }
    return [uint64]0
}

$PM_CPU_MASK = Get-PmCpuMask
$pmSelf = [System.Diagnostics.Process]::GetCurrentProcess()
[uint64]$pmCurrent = $pmSelf.ProcessorAffinity.ToInt64()
$pmBits = 0; for ($b = 0; $b -lt 64; ++$b) { if (($pmCurrent -shr $b) -band 1) { ++$pmBits } }
if ($PM_CPU_MASK -ne 0 -and $pmBits -gt $PM_JOBS) {
    $pmSelf.ProcessorAffinity = [IntPtr]::new([int64]$PM_CPU_MASK)  # never widened if already narrower
}
$PM_CPU_MASK = [uint64]$pmSelf.ProcessorAffinity.ToInt64()

$env:JAVA_HOME    = $PM_JDK
$env:VULKAN_SDK   = $PM_VKSDK
$env:ANDROID_HOME = $PM_SDK

# The owner's ~/.gitconfig points http.sslcainfo at a missing file: HTTPS uses the Windows store, per call.
function Invoke-PmGit {
    & $PM_GIT -c http.sslBackend=schannel -c advice.detachedHead=false @args
    if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') failed ($LASTEXITCODE)" }
}

# Known test devices (docs/PLAN.md 3.2): a name and the hardware serials it may be; the first attached wins.
$PM_DEVICES = @{
    moto    = @('ZY22BXMJG3')                                 # Motorola One 5G Ace, 60 Hz: the contract's floor
    moto2   = @('ZY22BZ8BDZ')
    tablet  = @('R5GL362BEHX', 'R5GL362AKGK', 'R5GL362AGPA')  # Galaxy Tab A11+, 90 Hz
    op12r   = @('a0e622a5')                                   # OnePlus 12R, 120 Hz (usually over Wi-Fi)
    fire    = @('GN41WE04450600BJ')                           # Fire 7 (2022): report only
}

# The hardware serial (ro.serialno) behind an adb serial (USB serials are already hardware serials).
$PM_DEVICE_IDS = @{}
function Get-PmDeviceId {
    param([Parameter(Mandatory)][string]$Serial)
    if ($Serial -notmatch '[:]|_adb-tls-connect') { return $Serial }
    if (-not $PM_DEVICE_IDS.ContainsKey($Serial)) {
        $id = "$(& $PM_ADB -s $Serial shell getprop ro.serialno 2>$null)".Trim()
        $PM_DEVICE_IDS[$Serial] = if ($id) { $id } else { $Serial -replace '[^\w.-]', '_' }
    }
    return $PM_DEVICE_IDS[$Serial]
}

# The adb serial for a device name or hardware serial: USB first, else a Wi-Fi transport whose ro.serialno
# matches, else one found by mDNS and connected. An ip:port given directly is returned as is.
function Resolve-PmSerial {
    param([string]$NameOrSerial)
    if ($NameOrSerial -match ':') { return $NameOrSerial }
    $wanted = @(if ($PM_DEVICES.ContainsKey($NameOrSerial)) { $PM_DEVICES[$NameOrSerial] } else { $NameOrSerial })
    $attached = @(& $PM_ADB devices 2>$null | Select-Object -Skip 1 |
        Where-Object { $_ -match '^(\S+)\s+device$' } | ForEach-Object { ($_ -split '\s+')[0] })
    foreach ($w in $wanted) { if ($attached -contains $w) { return $w } }
    $wifi = @($attached | Where-Object { $_ -match '[:]|_adb-tls-connect' })
    foreach ($w in $wanted) { foreach ($t in $wifi) { if ((Get-PmDeviceId $t) -eq $w) { return $t } } }
    $mdns = @(& $PM_ADB mdns services 2>$null)
    foreach ($w in $wanted) {
        $hit = $mdns | Where-Object { $_ -match "^adb-$([regex]::Escape($w))-\S*\s+_adb-tls-connect\._tcp\s+(\S+)" } |
            Select-Object -First 1
        if ($hit -and $hit -match '_adb-tls-connect\._tcp\s+(\S+)') {
            $addr = $Matches[1]
            & $PM_ADB connect $addr 2>&1 | Out-Null
            if ((& $PM_ADB -s $addr get-state 2>$null) -eq 'device') { Write-Host "[device] $w over Wi-Fi at $addr"; return $addr }
        }
    }
    return $wanted[0]
}

function Assert-PmDevice {
    param([Parameter(Mandatory)][string]$Serial)
    $state = (& $PM_ADB -s $Serial get-state 2>$null)
    if ($state -ne 'device') { throw "device $Serial is not attached (adb state: '$state')" }
}

function Get-PmForegroundPackage {
    param([Parameter(Mandatory)][string]$Serial)
    $line = (& $PM_ADB -s $Serial shell "dumpsys activity activities | grep -E 'mResumedActivity|topResumedActivity'") |
        Select-Object -First 1
    if ($line -match 'u0 ([\w\.]+)/') { return $Matches[1] }
    return ''
}

function Get-PmHomePackage {
    param([Parameter(Mandatory)][string]$Serial)
    $line = (& $PM_ADB -s $Serial shell 'cmd package resolve-activity --brief -a android.intent.action.MAIN -c android.intent.category.HOME') |
        Select-Object -Last 1
    return ($line -split '/')[0].Trim()
}

# --- Shared test devices (PLAN 3.3): other sessions use the same phones ------------------------------------
function Test-PmDeviceFree {
    param([Parameter(Mandatory)][string]$Serial)
    $fg = Get-PmForegroundPackage $Serial
    return ($fg -eq '' -or $fg -eq (Get-PmHomePackage $Serial) -or $fg -eq $PM_APP_ID)
}

function Wait-PmDeviceIdle {
    param([Parameter(Mandatory)][string]$Serial, [int]$QuietSeconds = 6, [int]$TimeoutSeconds = 300)
    $homePkg = Get-PmHomePackage $Serial
    $quietSince = $null
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $announced = $false
    while ((Get-Date) -lt $deadline) {
        $fg = Get-PmForegroundPackage $Serial
        if ($fg -eq '' -or $fg -eq $homePkg -or $fg -eq $PM_APP_ID) {
            if (-not $quietSince) { $quietSince = Get-Date }
            if (((Get-Date) - $quietSince).TotalSeconds -ge $QuietSeconds) { return $true }
        } else {
            if (-not $announced) { Write-Host "[device] $Serial is in use ($fg in front); waiting"; $announced = $true }
            $quietSince = $null
        }
        Start-Sleep -Seconds 2
    }
    return $false
}

# Device lock in the folder RAVEFORM's sessions use: first word = owner, file time = age, 30 minutes.
function Enter-PmDeviceLock {
    param([Parameter(Mandatory)][string]$Serial, [string]$Name = 'papermarch', [int]$WaitSeconds = 600)
    $lock = Join-Path $PM_LOCKS "$(Get-PmDeviceId $Serial).lock"
    $deadline = (Get-Date).AddSeconds($WaitSeconds)
    while ($true) {
        if (Test-Path $lock) {
            $age = ((Get-Date) - (Get-Item $lock).LastWriteTime).TotalMinutes
            $owner = "$(Get-Content $lock -Raw)".Trim()
            if ($age -lt 30 -and $owner -notmatch "^$([regex]::Escape($Name))\b") {
                if ((Get-Date) -gt $deadline) { throw "[lock] $Serial is busy: '$owner' ($([int]$age) min old)" }
                Write-Host "[lock] $Serial is busy: '$owner'; waiting"
                Start-Sleep -Seconds 15
                continue
            }
        }
        New-Item -ItemType Directory -Force $PM_LOCKS | Out-Null
        Set-Content -Path $lock -Value "$Name $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
        return $lock
    }
}

function Exit-PmDeviceLock {
    param([string]$Lock)
    if ($Lock -and (Test-Path $Lock)) { Remove-Item $Lock -Force -ErrorAction SilentlyContinue }
}

function Get-PmPid {
    param([Parameter(Mandatory)][string]$Serial)
    return "$(& $PM_ADB -s $Serial shell pidof $PM_APP_ID)".Trim()
}

function Stop-PmApp {
    param([Parameter(Mandatory)][string]$Serial)
    & $PM_ADB -s $Serial shell am force-stop $PM_APP_ID | Out-Null
}

# Starts the activity fresh and returns its PID. Refuses when another app is in front.
# Screen on and keyguard (without a PIN) out of the way, so a launch is shown (and `am start -W` returns).
function Invoke-PmWake {
    param([Parameter(Mandatory)][string]$Serial)
    & $PM_ADB -s $Serial shell input keyevent KEYCODE_WAKEUP | Out-Null
    if ((& $PM_ADB -s $Serial shell 'dumpsys window') | Select-String 'isKeyguardShowing=true' -Quiet) {
        & $PM_ADB -s $Serial shell wm dismiss-keyguard | Out-Null
        Start-Sleep -Milliseconds 800
    }
}
# Awake and unlocked, or a clear error: a phone with a PIN cannot be unlocked from here (the owner unlocks it).
function Assert-PmUnlocked {
    param([Parameter(Mandatory)][string]$Serial)
    Invoke-PmWake $Serial
    $awake = [bool]((& $PM_ADB -s $Serial shell 'dumpsys power') | Select-String 'mWakefulness=Awake' -Quiet)
    $locked = [bool]((& $PM_ADB -s $Serial shell 'dumpsys window') | Select-String 'isKeyguardShowing=true' -Quiet)
    if (-not $awake -or $locked) { throw "$Serial stays asleep or locked (a PIN?): unlock it by hand, then run again" }
}

# Keeps a phone from sleeping (and so from locking) during a long run, without touching its settings: every 8 s it wakes
# the screen and sends a Shift key (user activity, which restarts the screen timeout), only while PAPERMARCH or the launcher
# is in front. Returns a job for Stop-PmKeepAwake.
function Start-PmKeepAwake {
    param([Parameter(Mandatory)][string]$Serial)
    return Start-Job -ScriptBlock {
        param($adb, $serial, $app)
        while ($true) {
            & $adb -s $serial shell input keyevent KEYCODE_WAKEUP 2>$null | Out-Null
            $line = (& $adb -s $serial shell "dumpsys activity activities | grep -E 'mResumedActivity|topResumedActivity'" 2>$null) | Select-Object -First 1
            $fg = if ("$line" -match 'u0 ([\w\.]+)/') { $Matches[1] } else { '' }
            $homePkg = ((& $adb -s $serial shell 'cmd package resolve-activity --brief -a android.intent.action.MAIN -c android.intent.category.HOME' 2>$null) | Select-Object -Last 1) -split '/' | Select-Object -First 1
            if ($fg -eq $app -or $fg -eq "$homePkg".Trim()) {
                & $adb -s $serial shell input keyevent KEYCODE_SHIFT_LEFT 2>$null | Out-Null
            }
            Start-Sleep -Seconds 8
        }
    } -ArgumentList $PM_ADB, $Serial, $PM_APP_ID
}
function Stop-PmKeepAwake {
    param($Job)
    if ($Job) { Stop-Job $Job -ErrorAction SilentlyContinue; Remove-Job $Job -Force -ErrorAction SilentlyContinue }
}

function Start-PmApp {
    param([Parameter(Mandatory)][string]$Serial, [string]$AppArgs = '')
    if (-not (Test-PmDeviceFree $Serial)) {
        throw "$Serial is in use by another app ($(Get-PmForegroundPackage $Serial)); not launching over it"
    }
    Invoke-PmWake $Serial
    Stop-PmApp $Serial
    $cmd = "am start -W -n $PM_ACTIVITY"
    if ($AppArgs) { $cmd += " --es args '$AppArgs'" }
    & $PM_ADB -s $Serial shell $cmd | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "am start failed on $Serial" }
    Start-Sleep -Milliseconds 300
    return Get-PmPid $Serial
}
