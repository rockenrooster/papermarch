# PAPERMARCH — Progress

The owner's words that started the build (2026-11-26): "Ok, now implement. Public repo. You have my permission to do
anything you need (including elevated commands if needed)", then the goal "finish the project". Mid-way: "I want the
menus to be simple ... the map thing in the real game is not wanted at all. Just level after level until game is
finished (300 levels)" and "Next taps are fine". After two screenshots of the advertisement: "I'm still not seeing
anything close to it" and "I think you designed the wrong thing I was thinking of". The game was rebuilt as the
real-time squad shooter of those screenshots the same day (commit `fe44b21` on top of the turn-based `5b8bb05`).

## Status

| Part | Status |
|---|---|
| The battle (rules, bot, generator, 300 baked levels) | Done; the bot wins every level with the squad the bake recorded |
| The view (road, squad, gates, barrels, hordes, boss, effects) | Done on the PC and in the browser; not yet seen on a phone |
| Screens, flow, LEVELS, save, updater pill | Done; flow test 23/23; layout lint clean on 8 screens at 4 sizes |
| Audio | New battle sounds and cues; offline renders measured (below); the owner's ears pending |
| Android | The dev APK builds (4.9 MB); GitHub releases from `build-6` on (0.2.0, signed) are the shooter; the icon is a paper soldier on the road |
| Browser | Builds (index.wasm 2.8 MB); headless Chrome: the flow test 23/23, level 1 won by a touch sweeping the squad, NEXT, the save kept and read back, a notch's insets, 56 frames a second while the bot plays level 30 |
| Netlify | Not deployed: once, when the owner says the game is finished |

## Measured (this PC, 2026-11-26)

- The bake: 300 levels in about 16 s on 4 jobs (`captures/levels/report.md`); 0 failed.
- Unit tests 31 checks, the table 300/300, the flow test 23/23 (69 s with `--fixed-step`).
- Audio renders (`tools\wav_metrics.py`): the battle mix -21.9 LUFS, peak -7.2 dBFS, no clipping; the effects -24.7
  LUFS; the march -24.8 LUFS. Shot, hit and kill gains were raised after this measurement.
- Browser: 55.7-56.1 frames a second in headless Chrome at 1170 × 2532 on level 30.
- Desktop `bench_battle` on level 211 (the biggest hordes, 1,064 figures, 2,900 stickers at most): sim 0.04 ms, view
  0.25 ms, submit 0.07 ms a frame (P95), no heap allocation, 6 draw calls. The off-screen window's swap is throttled by
  Windows, so frame intervals on the PC mean nothing; the phones decide the contract.
- The view adds the squad, gates and barrels before the horde, so a full sticker pool (6,000) drops bullets and bits
  first; a horde over 400 or 700 alive draws its far members with fewer details.

## Choices made for the owner

| Question | Choice | Why |
|---|---|---|
| The game | A real-time squad shooter up a road, as in the owner's screenshots: drag to steer, automatic fire, gates that grow when shot, barrels, barricades, hordes, an ogre boss | The owner's screenshots and words |
| The look | Kept the cut-paper style (soldiers as paper pawns with folds and soft shadows) on a perspective stone road in daylight, ten worlds | Original, bright, nothing like GLOWTIDE; reads like the advertisement's layout |
| Steering | Relative drag: the squad moves by the finger's movement (1.25 road widths per road width dragged), anywhere on the screen | The finger never hides the squad |
| Difficulty | The bake's bands (section 6 of GAME_DESIGN): early levels are won with half the squad or more, late ones with 10-40% | A bot that plays like a careful player sets the bar |
| Stars | Three at 90% of the bot's end squad, two at 45% | Beating the bot is the chase |
| Version | 0.2.0 for the shooter | A different game from 0.1.0's prototype |

## Devices

On 2026-11-26 no test phone was attached or visible on Wi-Fi (`adb devices` empty). The phone checks (the benchmark,
captures, haptics, the updater on a real install) wait for a device.

## Open questions for the owner

- Is this the game from the advertisement now? What should change first?
- Steering: too fast or too slow (`--set steer=`)? Camera: more road ahead (`camPitch`, `camHeight`)?
- Difficulty and length (20-40 s a level): right?
- Sound: shots, explosions and the march on your phone.
- Netlify: say when it is finished and it goes live once.
