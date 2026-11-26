# PAPERMARCH

A one-thumb strategy puzzle for Android and the browser. Each level is a pop-up paper war map. Your host carries a
number; so does every camp. Tap a camp you can beat: its soldiers flip to your colours and join you. Pick your fights in
the right order, save the shrine for last, snowball from a dozen soldiers into a horde, and take the Keep. 300 levels,
ten worlds, level after level.

- **Android:** download `papermarch.apk` from the [latest release](https://github.com/rockenrooster/papermarch/releases/latest)
  (Android 10 or later, 64-bit ARM). The game shows an UPDATE button when a newer build is out.
- **Browser:** the same game built with Emscripten (WebAssembly + WebGL 2).

Everything you see and hear is made in code: the paper, the soldiers, the sound and the march music. The engine (C++20,
SDL 3, OpenGL ES 3.0) is forked from GLOWTIDE by the same owner. Font: Roboto (Apache 2.0); SDL 3 (zlib licence).
See `docs/` for the plan, the design and the progress log, and `CLAUDE.md` for the commands.
