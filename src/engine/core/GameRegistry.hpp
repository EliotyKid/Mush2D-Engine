#pragma once

#include "engine/core/IGame.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// One game built into this program. CMake (cmake/Games.cmake) writes the list; the engine only reads it.
struct GameEntry {
    std::string id;             // the name used by --game, for example "my_game"
    std::string name;           // the name shown to people
    std::filesystem::path dir;  // the game's folder, relative to the project root (for example "games/my_game")
    std::function<std::unique_ptr<IGame>()> create; // makes a new instance of the game's IGame class
};

// Every compiled game, in the order of the mush2d_add_game calls in CMakeLists.txt.
// Defined in the generated <build>/generated/games_list.cpp, so only programs that link the games have it.
const std::vector<GameEntry>& compiledGames();

// The compiled game with this id, or nullptr when there is none.
const GameEntry* findGame(std::string_view id);
