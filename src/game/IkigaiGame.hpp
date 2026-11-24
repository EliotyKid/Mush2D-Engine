#pragma once

#include "engine/core/FileWatcher.hpp"
#include "engine/core/IGame.hpp"
#include "engine/core/Paths.hpp"
#include "engine/world/Effects.hpp"
#include "engine/world/Prefab.hpp"
#include "engine/scenes/ISceneDefinition.hpp"
#include "engine/tools/LookPanel.hpp"
#include "engine/tools/SceneInspector.hpp"
#include "engine/world/LevelLoader.hpp"
#include "engine/world/RegionSet.hpp"
#include "engine/world/World.hpp"
#include "game/assets/GameAssets.hpp"
#include "game/scenes/SceneId.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

// The Ikigai game: the LDtk world (showcase.ldtk until the D12 demo map) with objects from prefabs, plus the
// older Sandbox and Test scenes (TAB). Registers the scene services the objects use: WorldState, World,
// GameAssets, AssetRegistry, the PrefabLibrary and the EffectLibrary.
class IkigaiGame : public IGame {
public:
    void init(EngineServices& s) override;
    void update(EngineServices& s, float dt) override;
    void onHud(EngineServices& s) override; // the test HUD (DebugHud) for the player's Resources
    void onImGui(EngineServices& s) override;
    void shutdown(EngineServices& s) override;

private:
    GameAssets gameAssets;
    std::vector<Material> atlasMaterials; // by atlas texture index (GameAtlasId)

    // Showcase room (LDtk)
    LevelLoader levelLoader;
    World world;                         // streams the rooms of showcase.ldtk around the player
    RegionSet regions;                   // assets/regions/<name>.region.json; the current room's look
    LookPanel lookPanel;
    SceneInspector inspector; // F1 "Cena"
    bool inspectorPause = false; // its "pausar o jogo" box
    bool panelShown = false;     // onImGui ran since the last update (F1 panel open)
    std::vector<EditableLight> lightEdits;   // one per LDtk Light entity of a loaded room
    GameObjectId playerObjectId = InvalidGameObjectId;

    float mouseZoomSensitivity = 0.15f;
    float cameraMoveSpeed = 1.5f;
    float cameraZoomSpeed = 1.0f;

    std::unordered_set<GameObjectId> activeTriggerOverlaps;
    std::unique_ptr<ISceneDefinition> activeSceneDefinition;

    SceneId currentSceneId = SceneId::Showcase;
    bool nextScenePressed = false;
    bool reloadScenePressed = false;
    bool killPressed = false;
    bool hurtPressed = false;  // debug J
    bool spendPressed = false; // debug L
    EventBus::Subscription diedSubscription; // Died -> killPlayer

    SceneTransitionData currentSceneTransitionData{};

private:
    std::pair<Material, glm::vec4> lookupAtlasFrame(std::uint32_t atlasIndex, const std::string& frameName) const;
    void submitRenderData(EngineServices& s);
    void handleGlobalInput(EngineServices& s, float deltaTime);
    void processTriggerOverlaps(EngineServices& s);
    void setupInitialScene(EngineServices& s);
    void registerLevelEntities();
    void loadShowcaseLevel(EngineServices& s);
    bool tryLoadScene(EngineServices& s, SceneId sceneId, const SceneTransitionData& transitionData);
    // Debug hot reload: a changed .ldtk or .region.json reloads the Showcase like F5, keeping the WorldState
    // and the player's place (pushed out up to 2 u, else the checkpoint).
    FileWatcher levelFiles; // showcase.ldtk and assets/regions/*.region.json
    FileWatcher prefabFiles; // assets/prefabs/*.prefab.json
    PrefabLibrary prefabs{sourceAssetPath("assets/prefabs")}; // Debug: the source tree, as the regions
    FileWatcher effectFiles; // assets/effects/*.effect.json: rescanned, no room reload (effects are found by name)
    EffectLibrary effects;
    bool keepRegions = false; // hot reload of the .ldtk alone keeps the in-memory regions (unsaved panel edits)
    void hotReload(EngineServices& s, bool ldtkOnly);
    std::optional<LdtkProject> preparedProject; // parsed by loadScene before any teardown
    void submitShowcaseExtras(EngineServices& s);
    glm::vec2 respawnPoint() const;
    void killPlayer(EngineServices& s, GameObjectId playerId); // death behind the fade (debug K)
    void reloadCurrentScene(EngineServices& s);
    void loadScene(EngineServices& s, SceneId sceneId, const SceneTransitionData& transitionData = {});
};
