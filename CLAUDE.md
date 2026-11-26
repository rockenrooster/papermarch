# PAPERMARCH — notes for Claude sessions

Planner's stub (2026-11-26): nothing is built yet. Phase P0 replaces this file with the real commands.

Read `docs/PLAN.md` first (its section 0 is the briefing), then `docs/PROGRESS.md` once it exists, then the subplan
of the phase you are executing (`docs/subplans/P0_fork_bootstrap.md` … `P7_release_web.md`). `docs/GAME_DESIGN.md`
and `docs/TECH_SPEC.md` by the sections a phase names. The look target is `docs/concept/concept_gameplay.png`.

## Owner's rules (they hold for every run)

- **No subagents**: one agent executes the plan directly.
- **PC runs are silent and off screen**, on **at most 4 CPU cores**. Phones keep their sound.
- **Shared devices** (PLAN 3): every adb command with `-s <serial>`; nothing device-wide; check the foreground app
  first; take the lock file in `D:\Stuff\DevFolder\Android_Games\Raveform\captures\locks\`; force-stop only
  `dev.papermarch.game`.
- Never edit, build or commit inside `..\Glowtide\` or `..\Raveform\` (GLOWTIDE is the fork's source: read and copy).
- Downloads, the public GitHub repository, pushes, the Netlify project and every production deploy need the owner's
  yes. The release key is the owner's to make. No destructive git or shell commands. Short chat replies.
