#include "EngineApp.hpp"
#include <iostream>

int main() {
    try {
        EngineApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}