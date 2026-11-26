#pragma once
// The app: SDL window and GL context, the frame loop with its fixed 60 Hz tick (TECH_SPEC 3.2), captures,
// benchmarks, lifecycle and input routing. Everything about the game itself lives in Game.

#include <SDL3/SDL.h>

#include "core/args.h"
#include "game/audio_out.h"
#include "game/bench.h"
#include "game/game.h"
#include "gfx/target.h"

namespace pm {

class App {
public:
    // Desktop and Android: start, frames until quit, finish; the exit code. In a browser (Emscripten): start, then the
    // browser calls frame() once a display frame (emscripten_set_main_loop) and run() returns 0 at once.
    int run(int argc, char** argv);

private:
    int start(int argc, char** argv);  // -1 when started, else the exit code
    void frame();
    int finish();
    bool initWindow();
    bool initGl();
    void readFrameGeometry();
    void pumpEvents();
    void handleEvent(const SDL_Event& e);
    static bool SDLCALL eventWatch(void* user, SDL_Event* e);
    bool captureFrame();
    void finishBench();

    Args args_;
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_ = nullptr;
    bool esContext_ = true;
    char glRenderer_[160] = "", glVersion_[160] = "", device_[96] = "desktop";
    int frameW_ = 0, frameH_ = 0, winW_ = 0, winH_ = 0;
    float dpScale_ = 1.0f;
    SafeArea safe_;
    bool useTarget_ = true;  // desktop renders into a framebuffer object sized by --res
    Target main_;

    Game game_;
    Bench bench_;
    AudioOut audio_;
    bool running_ = true;
    int exitCode_ = 0;
    bool background_ = false, firstFrameAfterResume_ = false;
    int frameIndex_ = 0;
    double startTime_ = 0;
    bool benchStarted_ = false;
    double benchT0_ = 0;
    bool pictureHeld_ = false;
    float refreshHz_ = 60.0f;
    double last_ = 0, acc_ = 0;  // the fixed tick's clock and backlog
    bool benchScenario_ = false;
    int shotFrame_ = 0, firstStripFrame_ = 0;
    i64 mouseDown_ = 0;
    float lastMouseX_ = 0, lastMouseY_ = 0;
};

}  // namespace pm
