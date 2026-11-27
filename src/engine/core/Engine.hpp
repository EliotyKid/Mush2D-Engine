#pragma once

#include "engine/core/GameRegistry.hpp"

class Engine {
public:
    // Points the asset paths at game.dir, creates the game, opens the window and runs until it closes.
    void run(const GameEntry& game);
};
