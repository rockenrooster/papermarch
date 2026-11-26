# P7 — Release and web

Size S. Needs P6 and the owner's sign-off.

**Goal.** The finished game is out both ways the owner asked for: the release APK through the GitHub pipeline, updating
installed copies by itself, and the browser build on Netlify. The Netlify deploy happens **once**, on the owner's
explicit yes.

## Read first

`PLAN.md` 2 (web hosting, repository) and 3 (Netlify). `docs/RELEASE_PIPELINE.md` (written in P3).
`..\Glowtide\docs\RELEASE_CHECKLIST.md` and, in `..\Glowtide\docs\PROGRESS.md`, "On Netlify" (what was checked on the
live site there).

## Why the Netlify step is careful

The owner's Netlify account is the free tier: 300 credits a month shared by every site on it (GLOWTIDE, RAVEFORM and
one more). A production deploy costs 15; a draft deploy costs nothing. When the credits run out, every site on the
account is paused. The owner's words on RAVEFORM: "You get one chance to publish". So: prove everything on a draft
deploy, then ask, then deploy to production once.

## Tasks

1. **The checklist**: `docs/RELEASE_CHECKLIST.md` from GLOWTIDE's, made true for this game; tick it line by line.
2. **The version**: ask the owner whether this is 1.0.0 or stays 0.x; set it in CMake and Gradle (they are kept equal).
3. **The APK**: push the release commit to `main`; watch the workflow (`gh run watch`); the release `build-<n>` has
   `papermarch.apk`, its `.sha256` and the symbols; its notes name the signing certificate. On the Moto: the installed
   previous GitHub build shows the UPDATE pill and updates on the owner's tap with its save kept.
4. **The browser build, final**: `scripts\build-web.ps1 -Clean`; `scripts\serve-web.ps1`; `web_check.mjs flow`, `play`
   and `reload` on `http://localhost:8092/`; `tablehash` in the browser equal to the PC's. Record `index.wasm`'s size.
5. **The Netlify project** (ask the owner first: "May I create the Netlify project `papermarch` on your account?"):
   `npx -y netlify-cli@27.20.1 sites:create --name papermarch` (if the name is taken: `papermarch-game`, and say so).
   The CLI is already signed in; if the sign-in has lapsed, `login --request` gives a link for the owner to authorize.
   `.netlify/` is git-ignored.
6. **A draft deploy** (free): `npx -y netlify-cli@27.20.1 deploy --no-build --dir build\web\site --site papermarch`.
   Against the draft's address: every file equal to the local build; the wasm served as `application/wasm`; the cache
   headers let a redeploy show at the next load; `web_check.mjs flow`, `play`, `reload`; `tablehash`. Ask the owner to
   open the draft on their phone.
7. **The production deploy** (ask the owner: "The draft passes every check: <address>. Deploy to production now? It
   costs 15 of the month's credits."). On their yes, once: the same command with `--prod`. Then the same checks on the
   live address.
8. **Wrap up**: the README's two links (the latest APK, the web address); `PROGRESS.md`'s final state, with "After
   version 1" for everything deferred; commit and push with `[skip ci]` if only documentation changed.

## Exit checks

- [ ] `RELEASE_CHECKLIST.md` is fully ticked, or each open line says who it waits on.
- [ ] The latest GitHub release is the release commit's build, signed with the owner's key; an update from the build
      before it was done on a device.
- [ ] The live web address passes `web_check.mjs flow`, `play` and `reload`, serves the same files as the local build,
      and its `tablehash` equals the PC's.
- [ ] Exactly one production deploy was made, after the owner's yes. If the owner has not said yes, the draft's address
      and its results are in `PROGRESS.md` and the phase waits there.
- [ ] Nothing secret or personal is in the repository or on the site.

## Owner checkpoint

The two yeses above (the project, the production deploy) and the version number. Afterwards: the two links, what was
measured, and the list of what is left for after version 1.

## Not in this phase

Anything new. The Play Store (a Play build must be built without `PM_BUILD_NUMBER`, so it has no updater:
`RELEASE_PIPELINE.md` says how; listing it is the owner's project).

## Notes

- A later fix to the game means another production deploy and another 15 credits: batch web fixes, and ask each time.
- Browser-only changes do not need an Android release: push them with `[skip ci]` (the workflow also ignores the
  browser's own files), as GLOWTIDE did.
