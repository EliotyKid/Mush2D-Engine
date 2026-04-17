#pragma once

#include "../../engine/world/GameObject.hpp"
#include "../../engine/world/Scene2D.hpp"

#include <string>

struct SceneTransitionData {
    GameObjectId preferredPlayerObjectId = InvalidGameObjectId;
    glm::vec2 playerSpawnPosition{0.0f, 0.0f};
    bool overridePlayerSpawn = false;

    std::string previousSceneName;
    std::string reason;
};

class ISceneDefinition {
public:
    struct BuildResult {
        GameObjectId playerObjectId = InvalidGameObjectId;
    };

public:
    virtual ~ISceneDefinition() = default;

    virtual const char* getSceneName() const = 0;

    virtual BuildResult build(Scene2D& scene, const SceneTransitionData& transitionData) = 0;

    virtual void onEnter(Scene2D& scene, const SceneTransitionData& transitionData) {}
    virtual void onExit(Scene2D& scene) {}
    virtual void onReload(Scene2D& scene) {}
    virtual void update(Scene2D& scene, float deltaTime) {}
};