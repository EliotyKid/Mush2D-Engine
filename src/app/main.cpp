#include "engine/core/Engine.hpp"
#include "engine/core/GameRegistry.hpp"

#include <iostream>
#include <string>
#include <string_view>

// Mush2D: the development program. It holds every compiled game and opens one of them.
//   Mush2D               -> the game list screen (the last game is marked)
//   Mush2D --game <id>   -> the game with that id, no screen (an unknown id lists the valid ones, then shows the screen)
int main(int argc, char** argv) {
    const auto& games = compiledGames();
    if (games.empty()) {
        std::cerr << "Mush2D: no game was compiled (see mush2d_add_game in CMakeLists.txt)." << std::endl;
        return -1;
    }

    // Reads "--game <id>" from the command line, if present.
    std::string wanted;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) != "--game") continue;
        if (i + 1 < argc) wanted = argv[i + 1];
        else std::cerr << "Mush2D: --game needs a game id. Showing the game list." << std::endl;
    }
    // An unknown id: list the valid ones and fall back to the launcher screen.
    if (!wanted.empty() && !findGame(wanted)) {
        std::cerr << "Mush2D: unknown game \"" << wanted << "\". Valid ids:";
        for (const GameEntry& g : games) std::cerr << ' ' << g.id;
        std::cerr << "." << std::endl;
        wanted.clear();
    }

    try {
        Engine engine;
        engine.run(games, wanted);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}
