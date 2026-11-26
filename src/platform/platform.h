#pragma once
// Platform specifics behind one small interface (TECH_SPEC 3.3, 3.5, 8.3): haptics, the panel's refresh rate,
// where output files go, and the display's present timestamps (Android EGL_ANDROID_get_frame_timestamps).

#include <cstddef>

#include "core/types.h"

struct SDL_Window;

namespace pm::platform {

bool isAndroid();
// Pins the calling thread to the CPUs with the highest capacity (Android big cores); returns how many (0: none).
int pinToFastCores(char* desc, size_t cap);
void init(SDL_Window* window);

// Haptic kinds (GAME_DESIGN 13): 0 light tick, 1 medium, 2 strong, 3 win pattern, 4 lose buzz, 5 double tick.
void haptic(int kind);

float refreshRateHz(SDL_Window* window);
// Asks the display for the mode nearest `hz` (Android; the one-way 60 Hz fallback of TECH_SPEC 5.10).
void requestRefreshRate(float hz);
// The updater (docs/RELEASE_PIPELINE.md): asks GitHub for the latest release's build number on a thread of its own
// (Android; a no-op on the desktop); latestBuild() is 0 until an answer arrives.
void checkForUpdate();
int latestBuild();
// HOME's UPDATE button: on Android the game downloads the latest release itself, checks it and opens Android's
// installer (the activity's startUpdate); on the desktop it opens the releases page. updateProgress() is the download's
// fraction, or -1 while nothing downloads.
void startUpdate();
float updateProgress();
constexpr const char* kReleasesUrl = "https://github.com/rockenrooster/papermarch/releases/latest";

// A browser build (Emscripten, web/shell.html). webCanvas() sets the page canvas's drawing buffer to its CSS size times
// devicePixelRatio and returns both sizes, the ratio and the safe area's insets (top, right, bottom, left; CSS pixels:
// a phone's notch and home bar). webReady() hides the page's loading line. restoreFile() and persistFile() keep a small
// file (the save) across visits in the browser's localStorage. Elsewhere isWeb() is false and the rest do nothing.
bool isWeb();
void webCanvas(int& pxW, int& pxH, int& cssW, int& cssH, float& ratio, float insets[4]);
void webReady();
void restoreFile(const char* path);
void persistFile(const char* path);

// A path for an output file: absolute paths stay; relative ones go to the app's external files directory on
// Android and stay relative (to the working directory) on the desktop.
void outputPath(const char* in, char* out, size_t n);

// Present timestamps. beforeSwap() once a frame before SDL_GL_SwapWindow; collect() returns display times (seconds)
// of frames that have been shown since the last call, in order. available() is false when the extension is missing.
void presentInit(SDL_Window* window);
bool presentAvailable();
void presentBeforeSwap();
int presentCollect(double* out, int max);

}  // namespace pm::platform
