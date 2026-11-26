# Third-party material

| Path | What | Origin | Author | Licence | Added |
|---|---|---|---|---|---|
| `third_party/SDL/` (not committed: `scripts/fetch-deps.ps1` clones it) | SDL 3.4.20 (`f00dc919ca35b9756316e86c4777392de442f9b2`) | github.com/libsdl-org/SDL | Sam Lantinga and contributors | zlib | 2026-11-26 (copied from GLOWTIDE's pin) |
| `assets/font/Roboto-Black.ttf`, `src/gfx/font_atlas.inc`, `src/gfx/font_metrics.inc` | Roboto Black, baked to a signed-distance atlas by `tools/bake_font.py` | the Android SDK's fonts (via GLOWTIDE) | Christian Robertson / Google | Apache 2.0 (`assets/font/LICENSE.txt`) | 2026-11-26 |
| `third_party/khronos/` (not committed) | OpenGL ES 3 headers | the Android NDK r29 | The Khronos Group | MIT-style (Khronos) | 2026-11-26 |

Everything else (the code, the shaders, every picture, sound and piece of music, the icon) was made for PAPERMARCH or
forked from GLOWTIDE, the same owner's game.
