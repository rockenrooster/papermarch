// Scripted pictures (TECH_SPEC 9.1): reproducible frames for the review sets, the device picture check and the strips.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "game/game.h"
#include "sim/levels_hand.h"

namespace pm {

namespace {
// The table's first level with this hint line (the hand-made levels carry theirs), else 0.
int levelWithHint(int hint) {
    for (int n = 1; n <= sim::levelCount(); ++n)
        if (sim::level(n).hint == hint) return n;
    return 0;
}
}  // namespace

bool Game::setupPicture(const char* name) {
    if (!std::strcmp(name, "concept")) {
        // The concept frame's twin (docs/concept): the growing camp's level after two moves (a camp, the village).
        const int n = levelWithHint(7);
        loadLevel(n > 0 ? n : 1);
        applyScriptedMoves(args_.moves[0] ? args_.moves : "1,2");
        return true;
    }
    if (!std::strcmp(name, "level")) {
        loadLevel(args_.level > 0 ? args_.level : 1);
        if (args_.moves[0]) applyScriptedMoves(args_.moves);
        return true;
    }
    if (!std::strcmp(name, "world")) {  // the first level of a world, at its start
        const int w = args_.world > 0 ? args_.world : 1;
        loadLevel(args_.level > 0 ? args_.level : (w - 1) * 10 + 1);
        return true;
    }
    if (!std::strcmp(name, "figures")) {  // a host of --set-like size: --level holds the host's number
        loadLevel(1);
        st_.s = args_.level > 0 ? args_.level : 400;
        scene_.snap(st_);
        return true;
    }
    if (!std::strcmp(name, "home_update")) {
        loadLevel(1);
        coldStart_ = true;
        titleFade_ = 1;
        updateOverride_ = 999;
        std::snprintf(pictureScreen_, sizeof pictureScreen_, "title");
        return true;
    }
    if (!std::strncmp(name, "screen_", 7)) {
        const char* s = name + 7;
        loadLevel(args_.level > 0 ? args_.level : 4);
        std::snprintf(pictureScreen_, sizeof pictureScreen_, "%s", s);
        if (!std::strcmp(s, "pause")) setScreen(Screen::Pause);
        else if (!std::strcmp(s, "settings")) setScreen(Screen::Settings);
        else if (!std::strcmp(s, "levels")) setScreen(Screen::Levels);
        else if (!std::strcmp(s, "end")) setScreen(Screen::End);
        else if (!std::strcmp(s, "win") || !std::strcmp(s, "lose")) {
            // Play it out: the best line for a win, the shrine first on level 4 for a loss.
            if (!std::strcmp(s, "lose")) {
                for (int i = 0; i < def_->siteCount; ++i)
                    if (def_->sites[i].kind == sim::kShrine) makeMove(i);
                while (!st_.over()) {
                    const u32 m = sim::takeable(*def_, st_);
                    int k = 0;
                    while (k < 31 && !(m & (1u << k))) ++k;
                    if (k >= 31) break;
                    makeMove(k);
                }
            } else {
                while (!st_.over() && solve_.first >= 0) makeMove(solve_.first);
            }
        } else if (!std::strcmp(s, "play")) {
            setScreen(Screen::Play);
        } else {
            return false;
        }
        return true;
    }
    return false;
}

}  // namespace pm
