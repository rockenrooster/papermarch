# PAPERMARCH — Plan

Status (2026-11-26): **built** as the real-time squad shooter the owner asked for. The first plan (a turn-based paper
war map) was the wrong game; the owner sent two screenshots of the advertisement and said "I think you designed the
wrong thing I was thinking of". That plan is kept in git history (commit `5b8bb05` and before) and in
`D:\Stuff\DevFolder\_backups\Papermarch\docs_map_design_2026-11-26\`. Everything below describes the game as it is now.

| Doc | Purpose |
|---|---|
| `PLAN.md` (this) | What the game is, the decisions, the environment and the rules for working here |
| `GAME_DESIGN.md` | The rules and their numbers, the level generator and the bake, stars, screens, look, sound |
| `TECH_SPEC.md` | The code: modules, the battle step, the camera, the renderer, audio, tools and tests |
| `PROGRESS.md` | What is built, what was measured, choices made for the owner, open questions |
| `RELEASE_PIPELINE.md` | The GitHub release per push and the in-app updater |
| `LICENSES.md` | Third-party pieces (SDL 3, the Roboto font) |

## 1. What we are building

The owner's words: a game that plays like the advertisements for a well-known mobile strategy game, not like that game;
simple and beautiful, awesome audio, an Android APK and a browser version (Netlify once it is fully complete), the
level selector and updater of GLOWTIDE (the owner's earlier game), and a look drastically different from GLOWTIDE.
Then: "the map thing in the real game is not wanted at all. Just level after level until game is finished (300
levels)" and "Next taps are fine". The advertisement, from the owner's screenshots: a blue squad at the foot of a paved
road fires up it without pause; the player steers it left and right; columns of blue +1 gates add soldiers, gold
barrels are shot open for recruits, red barricades take soldiers away; huge red hordes march down; a big boss waits at
the end; "Crush your enemies", "Grow your squad".

PAPERMARCH is that, as an original game in cut paper. A level is one run up a road, twenty to forty seconds long:

- The squad marches on its own and fires on its own. The player only drags left and right.
- **Gates** stand across the road in rows: blue `+N`, violet `×N`, red `-N` barricades. The squad passes the one it
  stands in. Bullets that hit a gate raise its number by one, up to a cap: shoot the gate you want, shoot a barricade
  until it is worth passing.
- **Barrels** carry recruits (`+N`). Shot open they join the squad; run into, they cost soldiers.
- **Hordes** of red soldiers (foot, fast riders, tough brutes) march down the road. Every one that reaches the squad
  takes soldiers with it. A big squad shreds a horde before it arrives; a small one is overrun.
- **The boss**, an ogre with a club, waits at the end of most later levels; the squad halts in front of it and fires
  until it falls, while the ogre crushes soldiers once it is close.
- Win: reach the finish line, or bring the boss down. Stars from the soldiers left. NEXT, and the next level.

Pillars: the snowball (the number climbs, the crowd grows, the music swells); one thumb and no friction (playing a
second after launch); beautiful and readable (bold paper in daylight colours, numbers legible at arm's length);
spectacle (bursts, falling figures, floating numbers, explosions, shake); never drops a frame.

## 2. Decisions

| Topic | Decision |
|---|---|
| Core | Real time, a fixed 60 Hz step (`src/sim/battle.*`). The player steers; everything else is automatic |
| Levels | 100 (the owner, 2026-11-27: "the last level can be 100"), each longer than the last, from half a minute to about 15 minutes; generated and baked: the bake plays each candidate with a bot and scales the hordes and the boss until the bot wins by the margin the level's place asks for. The table is committed (`src/sim/levels_baked.h`) |
| Flow | No hub, no world map. Launch plays the saved level; win: VICTORY with stars and NEXT (RETRY under three stars); defeat: RETRY and LEVELS; PAUSE: RESUME, RESTART, LEVELS, SETTINGS; after level 100 an end screen. The HUD's `LEVEL N` chip opens LEVELS (GLOWTIDE's selector) |
| Look | Cut paper in daylight on a perspective road: stone paving, curbs, a verge with trees and banners; ten worlds (meadow, desert, snow, autumn, beach, blossom, swamp, sky, volcano, lantern fair). Nothing like GLOWTIDE's glowing night |
| Teams | The squad blue, the horde red, barrels gold, multipliers violet; constant in every world |
| Engine | GLOWTIDE's shell, forked by copy: C++20, SDL 3.4.20, OpenGL ES 3.0 / WebGL 2, CMake, Gradle, Emscripten |
| Targets | Android arm64 (minSdk 29, targetSdk 36) and the browser; a Windows build for development |
| Release, updater | GitHub `rockenrooster/papermarch` (public); a signed release per push to `main`; the UPDATE pill (`RELEASE_PIPELINE.md`) |
| Web hosting | Netlify, **once, when the game is finished, on the owner's yes** (a production deploy costs 15 of the shared account's monthly credits). Until then this PC (port 8092) and free draft deploys |
| Money, network | No advertisements, purchases, analytics or accounts. The INTERNET permission only for the update check |
| Assets | All procedural: shapes, the road, sounds and music in code. Font: Roboto Black atlas (Apache 2.0) |

## 3. Environment

`..\Glowtide\docs\PLAN.md` section 3 describes this PC, its toolchain and the test devices. Differences:

| Item | Fact |
|---|---|
| Emscripten | 7.0.1 in `D:\Stuff\DevFolder\emsdk`, shared with RAVEFORM and GLOWTIDE: **only read it** |
| Ports | 8080 RAVEFORM, 8091 GLOWTIDE, **8092** PAPERMARCH. The server listens on IPv4: use `http://127.0.0.1:8092/` in tools |
| Netlify | `npx -y netlify-cli@27.20.1`, signed in. Drafts are free; `--prod` costs 15 credits and needs the owner's yes |
| Devices | Floor phone Motorola One 5G Ace `ZY22BXMJG3`; tablet Galaxy Tab A11+ `R5GL362BEHX`. Ask before the OnePlus 12R (`a0e622a5`) or the S25 Ultra (`R5CYA2CTX2Z`, `--user 0`) |
| Git | `C:\Program Files\Git\cmd\git.exe`; the push command is in `CLAUDE.md` |

**Shared test devices** (other sessions use the same phones): nothing device-wide (no `kill-server`, reboot,
`logcat -c`, settings); every adb command with `-s <serial>`; check the foreground app first; the lock file
`D:\Stuff\DevFolder\Android_Games\Raveform\captures\locks\<serial>.lock`; force-stop only `dev.papermarch.game` (and
`.dev`); keep runs short. Phones keep their sound; PC runs are silent and off screen on at most 4 cores.

## 4. Architecture

```
 touch ─▶ input (drag = where to steer) ─▶ Battle::tick (60 Hz: march, fire, bullets, gates, barrels, hordes, boss)
                                              │ BattleEvents (shots, kills, gate passed, barrel broken, ...)
                                              ▼
           Field: camera over the road, stickers far to near, foot shadows, tags, bursts, floating numbers, cues
                                              │                                  │ cues
                                              ▼                                  ▼
           Paper: ground pass (ground.frag) → union shadow → stickers → tags → UI     Feedback → Mixer (audio thread)
```

The battle never reads the view; the bot, the bake and the tests run the same steps with no view at all.

## 5. Performance contract

GLOWTIDE's definition: over 60 s after a warm-up, a frame is late when shown two or more refresh periods after the one
before. Late share ≤ 5% and worst frame ≤ 50 ms on the Motorola at 60 Hz (the gate); the tablet at 90 Hz if it holds.
The graded scene is `bench_battle` (the bot playing level 30 again and again). The browser check reports frames a second
(≥ 55 expected in headless Chrome). Main thread ≤ 4 ms P95; no heap allocation per frame; APK < 15 MB.

## 6. Working rules

- **Verify with numbers and pictures.** The executor cannot see motion or hear audio: scripted pictures
  (`--picture level --level N --fixed-step --frames F`), the flow test, the unit tests, offline audio renders with
  `tools\wav_metrics.py`. Never claim something looks, sounds or feels good unless the owner said so.
- **Small and direct.** No subagents unless the owner asks. Shortest working change; no scaffolding for later.
- **Content is data.** Levels are baked from `src/gen`; numbers that tune the feel are in `src/core/tuning.h`
  (`--set name=value`).
- **Git.** The repository is public: nothing secret or personal; `keys\` is gitignored. Pushes to `main` that change
  the Android build make a release; docs-only and browser-only pushes do not. No destructive git commands.
- **Backups** before a major change: `D:\Stuff\DevFolder\_backups\Papermarch\`.
