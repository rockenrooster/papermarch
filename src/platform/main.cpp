// Entry point. On Android, SDLActivity loads libmain.so and calls SDL_main() on its own thread.
#include <SDL3/SDL_main.h>

#include "game/app.h"

int main(int argc, char* argv[]) {
    static pm::App app;
    return app.run(argc, argv);
}
