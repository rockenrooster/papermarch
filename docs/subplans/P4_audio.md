# P4 — Audio

Size M. Needs P2 (the events). The owner asked for "awesome audio": this phase is not a garnish.

**Goal.** Every move has its sound, the count-up climbs a scale, and a five-layer march swells as the host closes on
its goal. Everything is synthesized. Worlds 1 to 3 have their music (the other seven come in P5). You cannot hear it:
you deliver measured renders, and the owner listens.

## Read first

`GAME_DESIGN.md` 10 and 12. `TECH_SPEC.md` 7 and 10. `..\Glowtide\docs\TECH_SPEC.md` 6 and, in
`..\Glowtide\docs\PROGRESS.md`, the rows "Mix levels", "The pad's voicing" and "Synthesis cost" (what GLOWTIDE learned
on these phones). `src/audio/mixer.h` and `synth.h` as forked.

## Tasks

1. **Take GLOWTIDE's music out** of the mixer (the pad set, `musicStep`, `setIntensity`'s meaning) and its sound
   table; keep the voices, the queue, the limiter, `play(sound, gain, pan, pitch, delay)`.
2. **The sound bank**: one recipe a row of GAME_DESIGN 12.2, plus the music's one-shots (frame drum, snare, rim click,
   low tom, cymbal swell, the bass note, the fife, the glockenspiel, the brass stab, the held fifth). Baked at start-up;
   time it on the Moto (at most 150 ms) and in the browser.
3. **The offline renderer** (`--render-audio`, TECH_SPEC 7) for every name, so each later task ends in a WAV and a row
   of `tools\wav_metrics.py`.
4. **The count ladder**: scale tables for the ten modes of GAME_DESIGN 12.1; a count-up's ticks climb, a count-down's
   descend; at most 24 ticks; `ladder.wav`.
5. **The sequencer** on the audio thread: the sixteenth clock, 128-step patterns a layer, pitch ratios from the mode,
   layer gains that slew over one bar. Patterns as constant tables: the theme of GAME_DESIGN 12.1, the oom-pah bass
   from the chords, the snare rudiments, the frame drum.
6. **Layers from the game**: `r = S / goal` after each move's count-up lands (not before: the swell is the reward);
   thresholds 0.10, 0.30, 0.60, 1.00 as tweakables.
7. **On the beat**: clash hits, stars and the fanfare's entry wait for the next sixteenth; taps and refusals never wait.
8. **Wire every event** of TECH_SPEC 3.3 and every UI action to its sound; the flip wave's rate limit (12 a second).
9. **Three themes for the owner**: `theme-a` (GAME_DESIGN 12.1 as written) and two of your own on the same chords
   (one more stepwise and calm, one with a dotted rhythm and wider leaps), each rendered with all five layers.
10. **Worlds 2 and 3**: their mode, tempo and lead voice; `music --world N` renders.
11. **The mix**: the effects and music gains, the limiter's ceiling. `battle.wav` is the reference render (a scripted
    busy fight over full music).
12. **Phones and the browser**: the audio callback's timing on the Moto (at most 25% of the buffer period at P99, no
    late callbacks: GLOWTIDE's bench reports it); sound starts in the browser after the first touch; SETTINGS' sound
    and music switches; the PC stays silent in every run.

## Exit checks (numbers from `wav_metrics.py`, in a table in `PROGRESS.md`)

- [ ] No render peaks above −1.5 dBFS; none clips.
- [ ] `battle.wav` at −16 ± 2 LUFS; `music --world 1 --layers 5` sits 9 ± 2 dB under it.
- [ ] Every music render and the ladder has at least 40% of its energy above 300 Hz.
- [ ] `ladder.wav`: the ticks' fundamentals rise strictly; the first and the twenty-fourth are two octaves apart.
- [ ] `music --world 1 --layers k` for k = 1..5: loudness rises with k; each layer's entry is on a bar line.
- [ ] The three worlds' renders differ: tempo and the set of pitch classes are as GAME_DESIGN 12.1's table says.
- [ ] The bank's bake time on the Moto and in the browser is inside its budget; the Moto's audio callback has no late
      call in a 60 s `bench_play`.
- [ ] The flow test's sound checks pass (music level changes with the switch; nothing plays with sound off).
- [ ] `bench_play` with sound: inside the contract, no regression, zero heap allocations a frame.

## Owner checkpoint (soft)

Send the WAVs (`theme-a`, `-b`, `-c`, `ladder`, `battle`, `win`, `perfect`, `lose`, `turn`, the three worlds) and the
build. Ask:

1. Which theme? Or none: what is wrong with them (too childish, too busy, too thin)?
2. The count-up: is the climb satisfying, or annoying after ten levels?
3. Do the layers coming in feel like a reward? Is layer 4 ("you can take the Keep") clear by ear?
4. Is anything harsh on the phone's speaker?

Record your own pick if they have not answered when the checks pass, and go on to P5.

## Not in this phase

Worlds 4 to 10's music, the boss cue's final form (both P5), haptics (P3).

## Notes

- The executor's ear is `wav_metrics.py` and arithmetic. Say so when reporting: "measured, not heard".
- A toy band gets grating when every note has the same velocity: give patterns two or three gain levels and the
  snare a ghost note.
- Do not let the count ladder and the melody fight: ladder ticks use the current chord's notes first, then the scale.
