#pragma once

#include "ISceneDefinition.hpp"
#include "SceneId.hpp"

#include <memory>

class SceneRegistry {
public:
    static std::unique_ptr<ISceneDefinition> create(SceneId sceneId);
};