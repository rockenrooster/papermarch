# Publishes the browser build (build\web\site) to GitHub Pages: the gh-pages branch of rockenrooster/papermarch, served at
# https://rockenrooster.github.io/papermarch/ (the owner, 2026-11-27: "can we publish to github pages too and have it
# work? if so then do it"). The page uses only relative paths and needs no special headers, so it works from that
# sub-path. Each publish is one commit on top of the branch (no force push).
#   scripts\publish-pages.ps1 [-Build]
param([switch]$Build)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\env.ps1"
$root = Split-Path $PSScriptRoot -Parent
if ($Build) {
    & "$PSScriptRoot\build-web.ps1"
    if ($LASTEXITCODE) { throw 'publish-pages: the web build failed' }
}
$site = Join-Path $root 'build\web\site'
if (-not (Test-Path (Join-Path $site 'index.html'))) { throw "publish-pages: no site in $site (run with -Build)" }
$git = 'C:\Program Files\Git\cmd\git.exe'
$auth = @('-c', 'http.sslBackend=schannel', '-c', 'http.sslcainfo=', '-c', 'credential.helper=', '-c', 'credential.helper=!gh auth git-credential')
$url = 'https://github.com/rockenrooster/papermarch.git'
$work = Join-Path $root 'build\pages'
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
# The branch as it is (or a new one), its files replaced by the site's.
& $git @auth clone --quiet --depth 1 --branch gh-pages $url $work 2>$null
if ($LASTEXITCODE) {
    New-Item -ItemType Directory -Force $work | Out-Null
    & $git -C $work init --quiet -b gh-pages
    & $git -C $work remote add origin $url
}
Get-ChildItem $work -Force | Where-Object { $_.Name -ne '.git' } | Remove-Item -Recurse -Force
Copy-Item (Join-Path $site '*') $work -Recurse
New-Item -ItemType File (Join-Path $work '.nojekyll') | Out-Null  # serve the files as they are
$version = (Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern 'project\(papermarch VERSION ([0-9.]+)').Matches[0].Groups[1].Value
& $git -C $work add -A
& $git -C $work commit --quiet -m "PAPERMARCH $version in the browser" 2>$null
if ($LASTEXITCODE) { Write-Host '[pages] nothing changed'; exit 0 }
& $git -C $work @auth push --quiet origin gh-pages
if ($LASTEXITCODE) { throw 'publish-pages: the push failed' }
Write-Host "[pages] PAPERMARCH $version published: https://rockenrooster.github.io/papermarch/"
