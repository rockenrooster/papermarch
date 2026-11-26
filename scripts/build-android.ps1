# Builds the APK with the Gradle wrapper (AGP 8.13.0, Gradle 8.13, NDK r29, SDK CMake 3.22.1) - TECH_SPEC 2.2, 2.4.
#   scripts\build-android.ps1 [-Config dev|debug|release] [-Offline] [-Clean]
# dev = native RelWithDebInfo, debuggable, debug-signed (the benchmarked build); debug = native Debug;
# release = native Release, not debuggable, no app arguments, debug-signed until the owner supplies a key.
# Output: android\app\build\outputs\apk\<config>\app-<config>.apk
# Shaders are validated by the desktop build's glslangValidator step before Gradle runs (CMake does it again).
# CPU limit: --max-workers=4 and a daemon of this project's own (immutable JVM options); never `gradlew --stop`.
param(
    [ValidateSet('dev', 'debug', 'release')][string]$Config = 'dev',
    [switch]$Offline,
    [switch]$Clean
)
. "$PSScriptRoot\env.ps1"

$task = ':app:assemble' + $Config.Substring(0, 1).ToUpper() + $Config.Substring(1)
$tasks = @()
if ($Clean) { $tasks += ':app:clean' }
$tasks += $task
$errorFile = Join-Path $GT_ROOT 'android\.gradle\hs_err_daemon.log'
$gradleArgs = @('--console=plain', "--max-workers=$GT_JOBS",
                "-Dorg.gradle.jvmargs=-Xmx2048m -Dfile.encoding=UTF-8 -XX:ActiveProcessorCount=$GT_JOBS -XX:ErrorFile=$errorFile")
if ($Offline) { $gradleArgs += '--offline' }
Push-Location (Join-Path $GT_ROOT 'android')
try {
    & .\gradlew.bat @tasks @gradleArgs
    if ($LASTEXITCODE -ne 0) { throw "gradle $($tasks -join ' ') failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
$apk = Join-Path $GT_ROOT "android\app\build\outputs\apk\$Config\app-$Config.apk"
Write-Host "[build] $apk ($([math]::Round((Get-Item $apk).Length / 1MB, 2)) MB)"
