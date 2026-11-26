// Scripted pictures: reproducible frames for the review sets, the device picture check and the strips. The bot plays
// (one step a frame with --fixed-step), so `--frames N` is N/60 seconds into the level.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "game/game.h"
#include "sim/bot.h"

namespace pm {

bool Game::setupPicture(const char* name) {
    if (!std::strcmp(name, "level") || !std::strcmp(name, "concept")) {
        loadLevel(args_.level > 0 ? args_.level : 12);
        useBot_ = true;
        return true;
    }
    if (!std::strcmp(name, "world")) {  // the first level of a world
        const int w = args_.world > 0 ? args_.world : 1;
        loadLevel(args_.level > 0 ? args_.level : (w - 1) * 10 + 1);
        useBot_ = true;
        return true;
    }
    if (!std::strcmp(name, "figures")) {  // a squad of --level soldiers at the start of level 1
        loadLevel(1);
        battle_.squad = args_.level > 0 ? args_.level : 150;
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
            // Play it out at once: the bot for a win; a squad of one held to the edge for a loss.
            const bool lose = !std::strcmp(s, "lose");
            for (int t = 0; t < 60 * 240 && !battle_.won && !battle_.lost; ++t) {
                if (lose) battle_.squad = 1;
                stepBattle(lose ? 4.9f : sim::botSteer(battle_), true);
            }
            setScreen(battle_.won ? Screen::Win : Screen::Lose);
            endTime_ = 10;
            resultTime_ = 3;
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
