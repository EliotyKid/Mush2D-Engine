#pragma once

#include "../world/GameObject.hpp"
#include "../world/Scene2D.hpp"

class ISceneDefinition {
public:
    struct BuildResult {
        GameObjectId playerObjectId = InvalidGameObjectId;
    };

public:
    virtual ~ISceneDefinition() = default;

    virtual BuildResult build(Scene2D& scene) = 0;
};