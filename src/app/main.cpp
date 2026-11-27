#include "engine/core/Engine.hpp"
#include "engine/core/GameRegistry.hpp"

#include <iostream>
#include <string>
#include <string_view>

// Mush2D: the development program. It holds every compiled game and opens one of them.
//   Mush2D               -> the first compiled game
//   Mush2D --game <id>   -> the game with that id (an unknown id lists the valid ones and opens the first)
int main(int argc, char** argv) {
    const auto& games = compiledGames();
    if (games.empty()) {
        std::cerr << "Mush2D: no game was compiled (see mush2d_add_game in CMakeLists.txt)." << std::endl;
        return -1;
    }

    // Reads "--game <id>" from the command line, if present.
    std::string wanted;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string_view(argv[i]) == "--game") wanted = argv[i + 1];

    const GameEntry* game = &games.front(); // default: the first game in the table
    if (!wanted.empty()) {
        if (const GameEntry* found = findGame(wanted)) {
            game = found;
        } else {
            std::cerr << "Mush2D: unknown game \"" << wanted << "\". Valid ids:";
            for (const GameEntry& g : games) std::cerr << ' ' << g.id;
            std::cerr << ". Opening \"" << game->id << "\"." << std::endl;
        }
    }

    try {
        Engine engine;
        engine.run(*game);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}
