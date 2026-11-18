#include "engine/core/Engine.hpp"
#include "game/ShowcaseGame.hpp"
#include <iostream>

int main() {
    try {
        ShowcaseGame game;
        Engine engine;
        engine.run(game);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}
