# The release key (docs/RELEASE_PIPELINE.md). Creates it in the project's keys\ (gitignored: never committed), gives it to
# the GitHub release workflow as its four secrets, and starts a release build. Interactively, keytool asks for a new
# password (twice) and this script once more. -Generate makes a random password instead and keeps it beside the
# keystore in keys\ (never printed, never on a command line: keytool and gh read it from the environment and stdin).
# Back the keystore and its password up: a build signed with another key cannot update an installed copy.
#   scripts\release-key.ps1 [-Keystore <path>] [-Generate] [-SecretsOnly] [-NoBuild]
param(
    [string]$Keystore = (Join-Path (Split-Path $PSScriptRoot) 'keys\papermarch-release.keystore'),
    [string]$Alias = 'papermarch',
    [string]$Repo = 'rockenrooster/papermarch',
    [switch]$Generate,     # a random password, kept in keys\ beside the keystore
    [switch]$SecretsOnly,  # the keystore exists: only set the secrets again
    [switch]$NoBuild       # set the secrets without starting a build
)
. "$PSScriptRoot\env.ps1"
$keytool = Join-Path $PM_JDK 'bin\keytool.exe'
$gh = (Get-Command gh -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source

function Set-GhSecret([string]$Name, [string]$Value) {
    # The value goes to gh's standard input as it is (no newline added, never on a command line).
    $psi = [Diagnostics.ProcessStartInfo]::new($gh)
    foreach ($a in @('secret', 'set', $Name, '-R', $Repo)) { $psi.ArgumentList.Add($a) }
    $psi.RedirectStandardInput = $true
    $psi.UseShellExecute = $false
    $p = [Diagnostics.Process]::Start($psi)
    $p.StandardInput.Write($Value)
    $p.StandardInput.Close()
    $p.WaitForExit()
    if ($p.ExitCode -ne 0) { throw "gh secret set $Name failed ($($p.ExitCode))" }
}

$pwFile = [IO.Path]::ChangeExtension($Keystore, '.password.txt')
if (-not $SecretsOnly) {
    if (Test-Path $Keystore) { throw "$Keystore exists and may be the key in use: keep it (-SecretsOnly uploads it again)" }
    New-Item -ItemType Directory -Force (Split-Path $Keystore) | Out-Null
    if ($Generate) {
        $bytes = [byte[]]::new(24)
        [Security.Cryptography.RandomNumberGenerator]::Fill($bytes)
        $env:PM_KEY_CHECK = [Convert]::ToBase64String($bytes).Replace('+', 'k').Replace('/', 'm').TrimEnd('=')
        [IO.File]::WriteAllText($pwFile, $env:PM_KEY_CHECK)
        Write-Host "[key] creating $Keystore with a random password (kept in $pwFile)"
        & $keytool -genkeypair -keystore $Keystore -storetype PKCS12 -alias $Alias -keyalg RSA -keysize 4096 -validity 10000 `
            -dname 'CN=PAPERMARCH' -storepass:env PM_KEY_CHECK -keypass:env PM_KEY_CHECK 2>&1 | Out-Null
    } else {
        Write-Host "[key] creating $Keystore (keytool asks for a new password, twice)"
        & $keytool -genkeypair -keystore $Keystore -storetype PKCS12 -alias $Alias -keyalg RSA -keysize 4096 -validity 10000 -dname 'CN=PAPERMARCH'
    }
    if ($LASTEXITCODE -ne 0) { throw 'keytool failed' }
}
if (-not (Test-Path $Keystore)) { throw "no keystore at $Keystore" }

$bstr = [IntPtr]::Zero
if ($Generate -or ($SecretsOnly -and (Test-Path $pwFile))) {
    $env:PM_KEY_CHECK = [IO.File]::ReadAllText($pwFile)
} else {
    $secure = Read-Host -AsSecureString "[key] the keystore's password, once more, for GitHub"
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    $env:PM_KEY_CHECK = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
}
try {
    $cert = & $keytool -list -v -keystore $Keystore -storepass:env PM_KEY_CHECK -alias $Alias 2>&1
    if ($LASTEXITCODE -ne 0) { throw "that password does not open $Keystore (or it has no key '$Alias')" }
    Set-GhSecret 'PM_KEYSTORE_BASE64' ([Convert]::ToBase64String([IO.File]::ReadAllBytes($Keystore)))
    Set-GhSecret 'PM_KEYSTORE_PASSWORD' $env:PM_KEY_CHECK
    Set-GhSecret 'PM_KEY_PASSWORD' $env:PM_KEY_CHECK  # (a PKCS12 keystore's key has the keystore's password)
    Set-GhSecret 'PM_KEY_ALIAS' $Alias
} finally {
    Remove-Item Env:PM_KEY_CHECK -ErrorAction SilentlyContinue
    if ($bstr -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr) }
}
$sha = ($cert | Select-String -Pattern 'SHA256:\s*(\S+)' | Select-Object -First 1).Matches.Groups[1].Value
$sha = $sha.Replace(':', '').ToLowerInvariant()  # (as the release notes print it)
Write-Host "[key] the four secrets are set on $Repo; the certificate's SHA-256: $sha"
Write-Host "[key] back up $Keystore and its password (not in the repository): without them no later build can update an installed copy"
if (-not $NoBuild) {
    gh workflow run release.yml -R $Repo --ref main
    if ($LASTEXITCODE -ne 0) { throw 'gh workflow run failed' }
    Write-Host "[key] a release build has started: gh run watch -R $Repo"
}
