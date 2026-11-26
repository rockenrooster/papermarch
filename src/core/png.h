#pragma once
// Minimal PNG writer (from RAVEFORM): 8-bit RGB, stored deflate blocks, CRC-32 and Adler-32. No third-party code.
// Screenshots are a debug action, so this may allocate.

#include "core/types.h"

namespace gt {

// `rgba` is tightly packed RGBA8. `bottomUp`: rows are stored bottom first (glReadPixels order).
bool writePng(const char* path, const u8* rgba, u32 width, u32 height, bool bottomUp);

}  // namespace gt
