# PAPERMARCH

A one-thumb squad shooter for Android and the browser, with painted toy soldiers in a sunlit diorama. Your little blue
squad marches up a stone road firing without pause. Drag it left and right: march through the blue gates (+1, +20,
×2), shoot the gate you want to make it bigger, shoot gold barrels open for recruits, shoot red barricades until they
are worth passing, and crush the red hordes coming down the road before they reach you. An ogre waits at the end.
Grow your squad, crush the horde. 100 levels in ten worlds, each longer than the last (from half a minute to about
fifteen), getting faster and harder, with hard levels thrown in; the last ones can only just be won.

- **Android:** download `papermarch.apk` from the [latest release](https://github.com/rockenrooster/papermarch/releases/latest)
  (Android 10 or later, 64-bit ARM). The game shows an UPDATE button when a newer build is out.
- **Browser:** play it at **https://rockenrooster.github.io/papermarch/**, the same game built with Emscripten
  (WebAssembly + WebGL 2). On a phone it goes full screen at the first touch (on an iPhone: Share, then Add to Home
  Screen).

Everything you see and hear is made by this repository's code: the soldiers, ogres, props and effects are 3D models
built and rendered by script in Blender (`tools/art/`, docs/ART_DIRECTION.md), the ground and the UI art by Python,
the sound and the march music by the synthesizer. The engine (C++20, SDL 3, OpenGL ES 3.0) is forked from GLOWTIDE by
the same owner. Font: Roboto (Apache 2.0); SDL 3 (zlib licence). See `docs/` for the plan, the design and the
progress log, and `CLAUDE.md` for the commands.
