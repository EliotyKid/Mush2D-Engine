#pragma once

#include "ISceneDefinition.hpp"

class TestScene : public ISceneDefinition {
public:
    const char* getSceneName() const override {
        return "TestScene";
    }

    BuildResult build(Scene2D& scene, const SceneTransitionData& transitionData) override;

    void onEnter(Scene2D& scene, const SceneTransitionData& transitionData) override;
    void onExit(Scene2D& scene) override;
    void onReload(Scene2D& scene) override;
    void update(Scene2D& scene, float deltaTime) override;

private:
    bool initialized = false;
    float elapsedTime = 0.0f;
    int pulseCycle = 0;
};