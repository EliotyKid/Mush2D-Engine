#pragma once

#include "engine/core/IGame.hpp"
#include "engine/scenes/ISceneDefinition.hpp"
#include "engine/world/LevelLoader.hpp"
#include "game/assets/GameAssets.hpp"
#include "game/scenes/SceneId.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

class ShowcaseGame : public IGame {
public:
    void init(EngineServices& s) override;
    void update(EngineServices& s, float dt) override;
    void shutdown(EngineServices& s) override;

private:
    GameAssets gameAssets;
    std::vector<Material> atlasMaterials; // by atlas texture index (GameAtlasId)

    // Showcase room (LDtk)
    struct Crystal { glm::vec2 anchor; int variant; };
    LevelLoader levelLoader;
    std::vector<GpuLight> levelLights;
    std::vector<Crystal> crystals;
    GameObjectId playerObjectId = InvalidGameObjectId;

    float mouseZoomSensitivity = 0.15f;
    float cameraMoveSpeed = 1.5f;
    float cameraZoomSpeed = 1.0f;

    std::unordered_set<GameObjectId> activeTriggerOverlaps;
    std::unique_ptr<ISceneDefinition> activeSceneDefinition;

    SceneId currentSceneId = SceneId::Showcase;
    bool nextScenePressed = false;
    bool reloadScenePressed = false;

    SceneTransitionData currentSceneTransitionData{};

private:
    std::pair<Material, glm::vec4> lookupAtlasFrame(std::uint32_t atlasIndex, const std::string& frameName) const;
    void submitRenderData(EngineServices& s);
    void handleGlobalInput(EngineServices& s, float deltaTime);
    void processTriggerOverlaps(EngineServices& s);
    void setupInitialScene(EngineServices& s);
    void registerLevelEntities();
    void loadShowcaseLevel(EngineServices& s);
    void submitShowcaseExtras(EngineServices& s);
    void reloadCurrentScene(EngineServices& s);
    void loadScene(EngineServices& s, SceneId sceneId, const SceneTransitionData& transitionData = {});
};
