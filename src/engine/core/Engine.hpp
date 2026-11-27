#pragma once

#include "engine/core/GameRegistry.hpp"

#include <string_view>
#include <vector>

class Engine {
public:
    // Opens the window, picks a game (cliGame if it names one, else the launcher screen), then runs it
    // until the window closes. The window and the Vulkan context are made once, for both.
    void run(const std::vector<GameEntry>& games, std::string_view cliGame);
};
