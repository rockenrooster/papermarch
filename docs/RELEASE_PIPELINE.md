# PAPERMARCH — the release pipeline (GitHub) and the updater

Set up on 2026-11-26 from GLOWTIDE's (its `docs/RELEASE_PIPELINE.md`), point for point; the owner asked for "the same
level selector and updater" and a public repository.

## Where

- The repository: `rockenrooster/papermarch` on GitHub, **public**, the remote `origin` of this checkout. Nothing secret
  or personal goes in a commit.
- The workflow: `.github/workflows/release.yml` ("Release APK").

## What runs, and when

- Every push to `main`, unless everything it changes is documentation (`docs/**`, any `*.md`) or the browser build's own
  files (`web/**`, `scripts/*-web.ps1`, `tools/serve_web.py`, `tools/web_check.mjs`). A push whose last commit's message
  holds `[skip ci]` runs nothing.
- By hand: `gh workflow run release.yml -R rockenrooster/papermarch`.
- One build at a time: a newer push cancels a build still running.

The job (`ubuntu-24.04`): JDK 17, the NDK `29.0.14206865`, CMake `3.22.1`, platform 36; `scripts/fetch-deps.sh` (SDL at
the pinned commit); `gradlew :app:assembleRelease` with `PM_BUILD_NUMBER` = the number of commits on `main`. The release
`build-<N>`, titled "PAPERMARCH <versionName> build <N>" and marked latest, holds `papermarch.apk`,
`papermarch.apk.sha256` and `papermarch-symbols.zip` (the unstripped libraries, for `ndk-stack`). The job checks the APK's
16 KB page alignment and prints the signing certificate in the notes.

## Downloading

- The newest build is always at `https://github.com/rockenrooster/papermarch/releases/latest/download/papermarch.apk`.
- On a phone: open the link, download, open the file; Android asks once to allow installs from the browser.

## Signing

The workflow signs with the key in four repository secrets (`PM_KEYSTORE_BASE64`, `PM_KEYSTORE_PASSWORD`,
`PM_KEY_ALIAS`, `PM_KEY_PASSWORD`; `android/app/build.gradle` reads them as `PM_KEYSTORE...`). The key was made before the
first public build (2026-11-26) with `scripts\release-key.ps1 -Generate`: the keystore is
`keys\papermarch-release.keystore` and its password `keys\papermarch-release.password.txt`, both on this PC only
(`keys\` is gitignored). **Back both up off this PC**: a build signed with any other key cannot update an installed copy.

This PC's builds (`scripts\deploy-android.ps1`) are debug-signed: a GitHub build and a deployed build replace each other
only through an uninstall. To test beside a GitHub build, build the dev APK with `PM_APP_ID_SUFFIX=.dev`.

## The updater

- Every GitHub build carries its build number (`cmake/BuildStamp.cmake` writes it into `build_stamp.h`); SETTINGS shows
  it. A local build has 0 and never asks.
- A build with a number asks GitHub once a launch (`platform::checkForUpdate`, `PapermarchActivity.checkForUpdate`): one
  request to `api.github.com/repos/rockenrooster/papermarch/releases/latest`, the number from `tag_name`. Nothing is
  sent but the request; no answer means no button. This is the only use of the INTERNET permission.
- While that number is higher than the build's own, the title card shows a gold `UPDATE: BUILD <n>` pill (and PAUSE
  shows it too). A tap downloads the APK in the game with its progress on the pill, checks its size, its SHA-256, the
  package, a higher `versionCode` and the same signing key, and hands it to Android's installer ("Do you want to update
  this app?"). The permission it needs (`REQUEST_INSTALL_PACKAGES`) and its receiver are in
  `android/app/src/github/AndroidManifest.xml`, merged only into a release built with a build number.
- `--picture home_update` shows the pill as if GitHub had build 999.
- A build for the Play store must not have it: build it without `PM_BUILD_NUMBER`.

## Version

`versionName` is `android/app/build.gradle`'s (kept equal to `CMakeLists.txt`'s project VERSION). `versionCode` is the
build number on GitHub builds and 1 on local builds.
