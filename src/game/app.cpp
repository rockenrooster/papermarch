#include "game/app.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "build_stamp.h"
#include "core/clock.h"
#include "core/log.h"
#include "core/tuning.h"
#include "gfx/gl.h"
#include "gfx/shader.h"
#include "platform/platform.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace gt {

namespace {

// dp scale of the desktop frames that stand for devices (TECH_SPEC 5.2; the Moto measured at 400 dpi).
float matrixDpScale(int w, int h) {
    struct Row { int w, h; float dp; };
    static const Row rows[] = {{1080, 2400, 2.5f}, {540, 1200, 1.25f}, {1080, 2640, 2.625f}, {1080, 1920, 2.625f},
                               {600, 1024, 1.0f},  {1200, 1920, 1.5f}, {1080, 1440, 2.0f}, {1264, 2780, 2.8125f}};  // the last: the OnePlus 12R (450 dpi)
    for (const Row& r : rows)
        if (r.w == w && r.h == h) return r.dp;
    return float(w) / 432.0f;
}

void* getProc(const char* name) { return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name)); }

}  // namespace

int App::run(int argc, char** argv) {
    const int failed = start(argc, argv);
    if (failed >= 0) return failed;
#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg([](void* app) { static_cast<App*>(app)->frame(); }, this, 0, false);
    return 0;
#else
    while (running_) frame();
    return finish();
#endif
}

int App::start(int argc, char** argv) {
    if (!parseArgs(argc, argv, args_)) return 2;
    startTime_ = nowSeconds();
    GT_LOG("GLOWTIDE %s (%s, %s), scenario %s", GT_VERSION, GT_COMMIT, GT_BUILD_DATE, args_.scenario);

    SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetAppMetadata("GLOWTIDE", GT_VERSION, "dev.glowtide.game");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        GT_ERR("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    if (!initWindow() || !initGl()) {
        SDL_Quit();
        return 1;
    }
    platform::init(window_);
    platform::presentInit(window_);
    char cpus[64];
    if (platform::pinToFastCores(cpus, sizeof cpus)) GT_LOG("cpu: main thread pinned to the fast cores %s", cpus);
    refreshHz_ = platform::refreshRateHz(window_);
    readFrameGeometry();
    GT_LOG("frame %dx%d px, dp scale %.3f, safe top %.0f bottom %.0f px, refresh %.1f Hz", frameW_, frameH_, dpScale_,
           safe_.top, safe_.bottom, refreshHz_);

    GameEnv env{frameW_, frameH_, dpScale_, safe_, platform::isAndroid(), refreshHz_};
    if (!game_.init(args_, env) || !game_.initGraphics()) {
        GT_ERR("start-up failed");
        SDL_Quit();
        return 1;
    }
    if (!args_.noAudio) {
        audio_.open(
            args_.silent, [](void* user, float* stereo, int frames) { static_cast<App*>(user)->game_.mixer().mix(stereo, frames); },
            this);
    }
    SDL_AddEventWatch(&App::eventWatch, this);
    platform::webReady();

    last_ = nowSeconds();
    acc_ = 0;
    benchScenario_ = !std::strncmp(args_.scenario, "bench_", 6);
    // Captures: the last frame is args_.frames; a strip adds K-1 earlier frames, stripStep apart.
    shotFrame_ = args_.frames;
    firstStripFrame_ = args_.strip > 1 ? shotFrame_ - (args_.strip - 1) * args_.stripStep : shotFrame_;
    return -1;
}

void App::frame() {
    if (!running_) return;
    {
        double& last = last_;
        double& acc = acc_;
        const bool benchScenario = benchScenario_;
        const int shotFrame = shotFrame_, firstStripFrame = firstStripFrame_;
        BenchFrame bf;
        const double t0 = nowSeconds();
        const double c0 = threadCpuSeconds();
        pumpEvents();
        if (!running_) return;
        if (background_) {
            if (!platform::isWeb()) SDL_Delay(10);  // a browser stops calling frame() for a hidden page itself
            last = nowSeconds();
            return;
        }
        const double t1 = nowSeconds();
        bf.phase[kPhaseEvents] = float(t1 - t0);

        if (args_.rebuildGlAt > 0 && frameIndex_ + 1 == args_.rebuildGlAt) {
            GT_LOG("rebuilding every GL object (--rebuild-gl-at %d)", args_.rebuildGlAt);
            game_.destroyGraphics();
            if (!game_.initGraphics()) {
                exitCode_ = 1;
                running_ = false;
                return;
            }
        }

        float dt = 0, alpha = 1;
        int ticks = 0;
        const double now = t1;
        if (args_.fixedStep) {
            dt = 1.0f / 60.0f;
            ticks = pictureHeld_ ? 0 : 1;
        } else {
            dt = float(clampv(now - last, 0.0, 0.1) * double(args_.speed));
            acc += dt;
            while (acc >= 1.0 / 60.0 && ticks < 4) {
                acc -= 1.0 / 60.0;
                ++ticks;
            }
            if (acc > 1.0 / 60.0) acc = 1.0 / 60.0;  // after a stall drop the backlog instead of racing
            alpha = float(acc * 60.0);
        }
        last = now;
        for (int i = 0; i < ticks; ++i) {
            game_.tick();
            bf.tickMax = maxv(bf.tickMax, game_.lastTickSeconds());
        }
        const double t2 = nowSeconds();
        bf.phase[kPhaseSim] = float(t2 - t1);
        if (!pictureHeld_) game_.update(dt, alpha);
        const double t3 = nowSeconds();
        bf.phase[kPhaseView] = float(t3 - t2);

        const unsigned fbo = useTarget_ ? main_.fbo : 0;
        game_.render(fbo, frameW_, frameH_);
        const double t4 = nowSeconds();
        bf.phase[kPhaseSubmit] = float(t4 - t3);
        ++frameIndex_;

        // Captures (before the swap: the back buffer still holds the frame on Android).
        if (shotFrame > 0 && args_.screenshot[0] && !pictureHeld_ && frameIndex_ >= firstStripFrame &&
            frameIndex_ <= shotFrame && (shotFrame - frameIndex_) % args_.stripStep == 0) {
            if (!captureFrame()) exitCode_ = 5;
            if (frameIndex_ == shotFrame) {
                if (platform::isAndroid() && args_.picture[0]) {
                    pictureHeld_ = true;  // a device keeps showing the picture for adb screencap
                    GT_LOG("picture: holding frame %d", shotFrame);
                } else {
                    running_ = false;
                }
            }
        } else if (shotFrame > 0 && !args_.screenshot[0] && frameIndex_ >= shotFrame) {
            running_ = false;
        }

        if (useTarget_ && !args_.offscreen) {
            int pw = 0, ph = 0;
            SDL_GetWindowSizeInPixels(window_, &pw, &ph);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, main_.fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, frameW_, frameH_, 0, 0, pw, ph, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        platform::presentBeforeSwap();
        const double t5 = nowSeconds();
        SDL_GL_SwapWindow(window_);
        const double t6 = nowSeconds();
        bf.phase[kPhaseSwap] = float(t6 - t5);
        bf.cpu = float(threadCpuSeconds() - c0);
        double presents[64];
        const int np = platform::presentCollect(presents, 64);
        game_.frameShown(float(t6 - t0));
        if (frameIndex_ == 1) GT_TAG("GTLIFE", "first frame swapped %.0f ms after main", (t6 - startTime_) * 1000.0);
        if (firstFrameAfterResume_) {
            GT_TAG("GTLIFE", "first frame after resume (frame %d)", frameIndex_);
            firstFrameAfterResume_ = false;
        }

        if (benchScenario) {
            if (!benchStarted_ && now - startTime_ >= double(args_.warmup)) {
                bench_.begin(args_.scenario, args_.seconds, now);
                audio_.resetStats();
                benchStarted_ = true;
                GT_LOG("bench: measuring %s for %.0f s", args_.scenario, double(args_.seconds));
            }
            if (bench_.active()) {
                if (benchT0_ == 0) benchT0_ = now;
                bf.interval = float(t6 - t0);
                bf.wisps = game_.wisps();
                bf.glooms = game_.glooms();
                bf.particles = game_.particles();
                bf.drawCalls = game_.drawCalls();
                bench_.addFrame(bf);
                bench_.addPresents(presents, np);
                if (now - benchT0_ >= double(args_.seconds)) {
                    finishBench();
                    running_ = false;
                }
            }
        }
        if (game_.quitRequested() && !platform::isWeb()) {  // a page is closed by the browser, not by the game
            running_ = false;
            if (game_.exitCode()) exitCode_ = game_.exitCode();
        }
    }
}

int App::finish() {
    SDL_RemoveEventWatch(&App::eventWatch, this);
    audio_.close();
    GT_LOG("haptics: %d sent, at most %d in one second", game_.hapticsSent(), game_.hapticPeakPerSecond());
    if (exitCode_ == 0 && game_.lintRan() && game_.lintProblems() > 0) {
        GT_ERR("layout lint: %d problems", game_.lintProblems());
        exitCode_ = 7;
    }
    if (exitCode_ == 0 && glDebugMessageCount() > 0) {
        GT_ERR("%d GL errors or debug messages were reported", glDebugMessageCount());
        exitCode_ = 3;
    }
    game_.destroyGraphics();
    main_.destroy();
    if (gl_) SDL_GL_DestroyContext(gl_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
    GT_LOG("exit %d after %d frames", exitCode_, frameIndex_);
    return exitCode_;
}

bool App::initWindow() {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
#if !defined(__ANDROID__) && !defined(NDEBUG)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    if (platform::isAndroid()) {
        window_ = SDL_CreateWindow("GLOWTIDE", 1080, 2400, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
        std::snprintf(device_, sizeof device_, "android");
    } else if (platform::isWeb()) {
        // The page's canvas (web/shell.html sizes it with CSS); its drawing buffer follows (readFrameGeometry).
        window_ = SDL_CreateWindow("GLOWTIDE", 432, 960, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        std::snprintf(device_, sizeof device_, "web");
    } else {
        // The frame (--res) is rendered into a framebuffer object; the window only shows it, scaled to fit.
        int ww = args_.resW, wh = args_.resH;
        const SDL_DisplayID display = SDL_GetPrimaryDisplay();
        SDL_Rect usable;
        if (SDL_GetDisplayUsableBounds(display, &usable) && wh > usable.h * 9 / 10) {
            const float s = float(usable.h * 9 / 10) / float(wh);
            ww = int(float(ww) * s);
            wh = int(float(wh) * s);
        }
        SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
        if (args_.offscreen) flags |= SDL_WINDOW_HIDDEN | SDL_WINDOW_UTILITY | SDL_WINDOW_NOT_FOCUSABLE;
        window_ = SDL_CreateWindow("GLOWTIDE", ww, wh, flags);
        if (window_ && args_.offscreen) {
            SDL_SetWindowPosition(window_, -ww - 400, 0);
            GT_LOG("window: off screen and hidden (--offscreen)");
        }
    }
    if (!window_) {
        GT_ERR("window: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool App::initGl() {
    gl_ = SDL_GL_CreateContext(window_);
    if (!gl_ && !platform::isAndroid()) {
        GT_WARN("no OpenGL ES 3.0 context (%s); trying OpenGL 3.3 core", SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        gl_ = SDL_GL_CreateContext(window_);
        esContext_ = false;
        setGlslVersionLine("#version 330 core\n");
    }
    if (!gl_) {
        GT_ERR("GL context: %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(window_, gl_);
    if (!loadGl(getProc)) return false;
    SDL_GL_SetSwapInterval(1);
#if !defined(NDEBUG)
    installGlDebug(getProc);
#endif
    std::snprintf(glRenderer_, sizeof glRenderer_, "%s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    std::snprintf(glVersion_, sizeof glVersion_, "%s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    for (char* p = glRenderer_; *p; ++p)
        if (*p == '"' || *p == '\\') *p = ' ';
    for (char* p = glVersion_; *p; ++p)
        if (*p == '"' || *p == '\\') *p = ' ';
    GT_LOG("GL: %s context; vendor %s; renderer %s; version %s; GLSL %s", esContext_ ? "ES 3.0" : "OpenGL 3.3 core",
           glGetString(GL_VENDOR), glRenderer_, glVersion_, glGetString(GL_SHADING_LANGUAGE_VERSION));
    GLint n = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    char line[1024];
    int o = 0;
    for (GLint i = 0; i < n; ++i) {
        const char* e = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, GLuint(i)));
        const int len = int(std::strlen(e));
        if (o + len + 2 >= int(sizeof line)) {
            GT_LOG("GL extensions: %s", line);
            o = 0;
        }
        o += std::snprintf(line + o, sizeof line - size_t(o), "%s ", e);
    }
    if (o) GT_LOG("GL extensions: %s", line);
    return true;
}

void App::readFrameGeometry() {
    if (platform::isAndroid()) {
        SDL_GetWindowSizeInPixels(window_, &frameW_, &frameH_);
        int ww = 0, wh = 0;
        SDL_GetWindowSize(window_, &ww, &wh);
        dpScale_ = args_.dpScale > 0 ? args_.dpScale : SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window_));
        if (dpScale_ <= 0) dpScale_ = 2.5f;
        SDL_Rect safe;
        const float pxPerPoint = ww > 0 ? float(frameW_) / float(ww) : 1.0f;
        if (SDL_GetWindowSafeArea(window_, &safe)) {
            safe_.top = float(safe.y) * pxPerPoint;
            safe_.left = float(safe.x) * pxPerPoint;
            safe_.bottom = float(wh - safe.y - safe.h) * pxPerPoint;
            safe_.right = float(ww - safe.x - safe.w) * pxPerPoint;
        }
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_));
        if (mode) {
            GT_LOG("surface %dx%d %s the display mode %dx%d", frameW_, frameH_,
                   (mode->w == frameW_ && mode->h == frameH_) || (mode->w == frameH_ && mode->h == frameW_) ? "matches"
                                                                                                            : "DIFFERS from",
                   mode->w, mode->h);
        }
        useTarget_ = false;
    } else if (platform::isWeb()) {
        // The canvas's drawing buffer is its CSS size times devicePixelRatio, which is the dp scale (a CSS pixel is about
        // a dp); the safe area is the page's env(safe-area-inset-*): a phone's notch and home bar.
        float insets[4];  // top, right, bottom, left: CSS pixels
        platform::webCanvas(frameW_, frameH_, winW_, winH_, dpScale_, insets);
        safe_.top = insets[0] * dpScale_;
        safe_.right = insets[1] * dpScale_;
        safe_.bottom = insets[2] * dpScale_;
        safe_.left = insets[3] * dpScale_;
        useTarget_ = false;
    } else {
        frameW_ = args_.resW;
        frameH_ = args_.resH;
        dpScale_ = args_.dpScale > 0 ? args_.dpScale : matrixDpScale(frameW_, frameH_);
        safe_.top = args_.safeTop * dpScale_;
        safe_.bottom = args_.safeBottom * dpScale_;
        useTarget_ = true;
        main_.create(frameW_, frameH_, true, true);
        SDL_GetWindowSize(window_, &winW_, &winH_);
    }
}

void App::pumpEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) handleEvent(e);
}

bool SDLCALL App::eventWatch(void* user, SDL_Event* e) {
    App* self = static_cast<App*>(user);
    switch (e->type) {
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            GT_TAG("GTLIFE", "will enter background");
            self->background_ = true;
            self->game_.background();
            self->audio_.pause(true);
            if (self->bench_.active()) {
                self->bench_.focusLost();
                GT_WARN("bench: the app went to the background; the run is invalid");
            }
            break;
        case SDL_EVENT_DID_ENTER_BACKGROUND:
            GT_TAG("GTLIFE", "background");
            break;
        case SDL_EVENT_WILL_ENTER_FOREGROUND:
            GT_TAG("GTLIFE", "will enter foreground");
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            GT_TAG("GTLIFE", "foreground");
            self->background_ = false;
            self->firstFrameAfterResume_ = true;
            self->game_.foreground();
            self->audio_.pause(false);
            break;
        default:
            break;
    }
    return true;
}

void App::handleEvent(const SDL_Event& e) {
    const float sx = winW_ > 0 ? float(frameW_) / float(winW_) : 1.0f;
    const float sy = winH_ > 0 ? float(frameH_) / float(winH_) : 1.0f;
    switch (e.type) {
        case SDL_EVENT_QUIT:
            running_ = false;
            break;
        case SDL_EVENT_RENDER_DEVICE_RESET:
            GT_TAG("GTLIFE", "GL context was lost: rebuilding every GL object");
            game_.destroyGraphics();
            if (!game_.initGraphics()) {
                exitCode_ = 1;
                running_ = false;
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (bench_.active()) {
                bench_.focusLost();
                GT_WARN("bench: focus lost; the run is invalid");
            }
            break;
        case SDL_EVENT_WINDOW_RESIZED:  // a page: the browser's window, a phone turned, Safari's bars shown or hidden
            if (!platform::isWeb()) break;
            [[fallthrough]];
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
            if (platform::isAndroid() || platform::isWeb()) {
                readFrameGeometry();
                game_.resize({frameW_, frameH_, dpScale_, safe_, platform::isAndroid(), refreshHz_});
                // Again, with the insets the window has now: the device picture check reads the last such line.
                GT_LOG("frame %dx%d px, dp scale %.3f, safe top %.0f bottom %.0f px, refresh %.1f Hz", frameW_, frameH_, dpScale_,
                       safe_.top, safe_.bottom, refreshHz_);
            }
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP: {
            PointerEvent p;
            p.type = e.type == SDL_EVENT_FINGER_DOWN ? PtrType::Down : e.type == SDL_EVENT_FINGER_UP ? PtrType::Up : PtrType::Move;
            p.id = i64(e.tfinger.fingerID);
            p.x = e.tfinger.x * float(frameW_);
            p.y = e.tfinger.y * float(frameH_);
            p.dx = e.tfinger.dx * float(frameW_);
            p.dy = e.tfinger.dy * float(frameH_);
            game_.pointer(p);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT) {
                PointerEvent p;
                p.type = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? PtrType::Down : PtrType::Up;
                p.id = -1;
                p.x = e.button.x * sx;
                p.y = e.button.y * sy;
                p.dx = p.dy = 0;
                mouseDown_ = p.type == PtrType::Down;
                lastMouseX_ = p.x;
                lastMouseY_ = p.y;
                game_.pointer(p);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (e.motion.which != SDL_TOUCH_MOUSEID && mouseDown_) {
                PointerEvent p;
                p.type = PtrType::Move;
                p.id = -1;
                p.x = e.motion.x * sx;
                p.y = e.motion.y * sy;
                p.dx = p.x - lastMouseX_;
                p.dy = p.y - lastMouseY_;
                lastMouseX_ = p.x;
                lastMouseY_ = p.y;
                game_.pointer(p);
            }
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            const bool down = e.type == SDL_EVENT_KEY_DOWN;
            if (down && e.key.repeat) break;
            switch (e.key.key) {
                case SDLK_ESCAPE:
                case SDLK_AC_BACK: game_.key(Key::Back, down); break;
                case SDLK_P: game_.key(Key::Pause, down); break;
                case SDLK_A:
                case SDLK_LEFT: game_.key(Key::Left, down); break;
                case SDLK_D:
                case SDLK_RIGHT: game_.key(Key::Right, down); break;
                case SDLK_SPACE: game_.key(Key::Fire, down); break;
                case SDLK_F1: game_.key(Key::Debug, down); break;
                default: break;
            }
            break;
        }
        default:
            break;
    }
}

bool App::captureFrame() {
    char name[300];
    if (frameIndex_ == args_.frames) {
        std::snprintf(name, sizeof name, "%s", args_.screenshot);
    } else {
        // <base>_<k>.png for the strip's earlier frames, k = 1 .. K-1 in time order.
        const int k = args_.strip - (args_.frames - frameIndex_) / args_.stripStep;
        char base[260];
        std::snprintf(base, sizeof base, "%s", args_.screenshot);
        if (char* dot = std::strrchr(base, '.')) *dot = 0;
        std::snprintf(name, sizeof name, "%s_%d.png", base, k);
    }
    char path[512];
    platform::outputPath(name, path, sizeof path);
    glBindFramebuffer(GL_FRAMEBUFFER, useTarget_ ? main_.fbo : 0);
    return captureBound(path, frameW_, frameH_);
}

void App::finishBench() {
    char extra[512];
    const AudioStats a = audio_.stats();
    std::snprintf(extra, sizeof extra,
                  ",\"frame\":\"%dx%d\",\"dp_scale\":%.3f,\"audio\":{\"open\":%s,\"silent\":%s,\"callbacks\":%d,"
                  "\"period_ms\":%.2f,\"fill_ms\":[%.3f,%.3f,%.3f],\"gap_ms\":[%.2f,%.2f,%.2f],\"late\":%d},"
                  "\"haptics\":{\"sent\":%d,\"peak_per_s\":%d}",
                  frameW_, frameH_, dpScale_, audio_.isOpen() ? "true" : "false", args_.silent ? "true" : "false",
                  a.callbacks, double(a.periodMs), double(a.fillMs[0]), double(a.fillMs[1]), double(a.fillMs[2]),
                  double(a.gapMs[0]), double(a.gapMs[1]), double(a.gapMs[2]), a.late, game_.hapticsSent(),
                  game_.hapticPeakPerSecond());
    bench_.finish(device_, glRenderer_, glVersion_, refreshHz_, extra);
    if (!bench_.valid()) exitCode_ = 4;
}

}  // namespace gt
