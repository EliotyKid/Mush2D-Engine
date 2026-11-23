#include "engine/core/Engine.hpp"
#include "game/RegisterGameTypes.hpp"
#include "game/IkigaiGame.hpp"
#include <iostream>

int main() {
    mush2dRegisterGameTypes();
    try {
        IkigaiGame game;
        Engine engine;
        engine.run(game);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}
