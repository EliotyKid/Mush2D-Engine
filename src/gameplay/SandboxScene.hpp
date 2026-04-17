#pragma once

#include "../world/GameObject.hpp"
#include "../world/Scene2D.hpp"

class SandboxScene {
public:
    struct BuildResult {
        GameObjectId playerObjectId = InvalidGameObjectId;
    };

public:
    static BuildResult build(Scene2D& scene);
};