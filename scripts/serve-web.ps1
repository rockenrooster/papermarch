# Serves the browser build to phones on this PC's network (an iPhone opens the address it prints in Safari).
#   scripts\serve-web.ps1 [-Port 8091] [-Build] [-MoveTo https://glowtide.netlify.app/]
# -Build runs scripts\build-web.ps1 first. Ctrl+C stops the server. Port 8091 (RAVEFORM's web build uses 8080).
# -MoveTo: the game is hosted elsewhere now; every visit here is sent there with the save this address kept.
# Phones reach it only once Windows Firewall lets the port in (this PC does not ask): once, in an administrator
# PowerShell: New-NetFirewallRule -DisplayName 'GLOWTIDE web server (8091)' -Direction Inbound -Protocol TCP
#   -LocalPort 8091 -Action Allow -Profile Private
param(
    [int]$Port = 8091,
    [switch]$Build,
    [string]$MoveTo
)
. "$PSScriptRoot\env.ps1"
if ($Build) { & "$PSScriptRoot\build-web.ps1" }
if ($MoveTo) {
    & python -I -B (Join-Path $GT_ROOT 'tools\serve_web.py') --port $Port --move-to $MoveTo
    exit $LASTEXITCODE
}

Write-Host "[serve] on this PC: http://localhost:$Port/"
Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object { $_.IPAddress -ne '127.0.0.1' -and $_.IPAddress -notlike '169.254.*' -and $_.InterfaceAlias -notlike 'vEthernet*' } |
    ForEach-Object {
        $category = (Get-NetConnectionProfile -InterfaceAlias $_.InterfaceAlias -ErrorAction SilentlyContinue).NetworkCategory
        Write-Host ("[serve] on a phone on the network of {0} ({1}): http://{2}:{3}/" -f $_.InterfaceAlias, $category, $_.IPAddress, $Port)
    }
& python -I -B (Join-Path $GT_ROOT 'tools\serve_web.py') --port $Port
