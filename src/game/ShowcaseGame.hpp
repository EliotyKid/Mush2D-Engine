#pragma once

#include "engine/core/IGame.hpp"
#include "engine/scenes/ISceneDefinition.hpp"
#include "engine/tools/LookPanel.hpp"
#include "engine/world/LevelLoader.hpp"
#include "engine/world/Region.hpp"
#include "engine/world/World.hpp"
#include "game/assets/GameAssets.hpp"
#include "game/scenes/SceneId.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

class ShowcaseGame : public IGame {
public:
    void init(EngineServices& s) override;
    void update(EngineServices& s, float dt) override;
    void onImGui(EngineServices& s) override;
    void shutdown(EngineServices& s) override;

private:
    GameAssets gameAssets;
    std::vector<Material> atlasMaterials; // by atlas texture index (GameAtlasId)

    // Showcase room (LDtk)
    struct Crystal { std::string iid; glm::vec2 anchor; int variant; std::string roomIid; };
    LevelLoader levelLoader;
    World world;                         // streams the rooms of showcase.ldtk around the player
    std::map<std::string, Region> regions; // assets/regions/<name>.region.json, loaded on first use
    std::string currentRegion;           // region of the current room; the panel edits and saves it
    FrameSettings blendFrom;             // look shown when the region changed
    float blendTime = 0.0f;
    static constexpr float RegionBlendSeconds = 1.5f;
    LookPanel lookPanel;
    std::vector<EditableLight> lightEdits;   // one per LDtk Light entity of a loaded room
    std::vector<EditableLayer> layerEdits;   // one per static tile layer
    std::vector<Crystal> crystals;           // of loaded rooms
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
    void tryLoadScene(EngineServices& s, SceneId sceneId, const SceneTransitionData& transitionData);
    std::optional<LdtkProject> preparedProject; // parsed by loadScene before any teardown
    void submitShowcaseExtras(EngineServices& s);
    Region& region(const std::string& name);
    std::string roomRegion(const std::string& roomIid) const;
    void updateRegion(EngineServices& s, float dt);
    void saveRegions();
    void reloadCurrentScene(EngineServices& s);
    void loadScene(EngineServices& s, SceneId sceneId, const SceneTransitionData& transitionData = {});
};
