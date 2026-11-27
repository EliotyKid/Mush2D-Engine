#include "GameRegistry.hpp"

const GameEntry* findGame(std::string_view id) {
    // A plain search: there are only a few compiled games.
    for (const GameEntry& game : compiledGames())
        if (game.id == id) return &game;
    return nullptr;
}
