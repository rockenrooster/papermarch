#pragma once
// Offline renders of the game's sound (TECH_SPEC 7): a WAV file per name, measured with tools/wav_metrics.py.

namespace pm {

// name: ladder | battle | ui | win | lose | turn | music (world, layers, boss) | flip | effects. 0 = written.
int renderAudio(const char* name, const char* outPath, int world, int layers, bool boss, float seconds);
bool writeWav(const char* path, const float* stereo, int frames, int rate);

}  // namespace pm
