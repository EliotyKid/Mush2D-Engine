#include "game/IkigaiGame.hpp"

#include "engine/core/Paths.hpp"
#include "engine/render/LightComponent.hpp"
#include "engine/world/Collision.hpp"
#include "engine/world/IPlayer.hpp"
#include "engine/world/Resources.hpp"
#include "game/objects/IkiObject.hpp"
#include "game/scenes/SceneRegistry.hpp"
#include "game/ui/DebugHud.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

// The player is the object tagged "player" (spec D2 decision 4): the remembered one, else the first one found.
static GameObject* livePlayer(Scene2D& scene, GameObjectId& id) {
    GameObject* player = scene.findObject(id);
    if (!player || !player->hasTag("player")) {
        player = findPlayer(scene);
        if (player) {
            id = player->id;
        }
    }
    return player;
}

void IkigaiGame::init(EngineServices& s) {
    gameAssets.registerDefaults();
    // Objects reach the game's data through scene services (spec decision 5), not constructor arguments.
    s.scene.setService(&gameAssets);
    s.scene.setService(&gameAssets.registry);
    s.scene.setService(&prefabs);
    effects.scan(sourceAssetPath("assets/effects"));
    s.scene.setService(&effects);
    s.scene.setService(&world);
    s.scene.setService(&world.state);
    // Health 0 (Resources emits Died): the existing death (fade, room restore, checkpoint).
    diedSubscription = s.scene.events().subscribe<Died>([this, &s](const Died& e) {
        if (currentSceneId == SceneId::Showcase && e.id == playerObjectId) killPlayer(s, e.id);
    });
    regions.init(s.materials, sourceAssetPath("assets/regions")); // Debug: the source tree, so panel saves survive rebuilds

    atlasMaterials.clear();
    for (const AssetRegistry::Atlas& atlas : gameAssets.registry.atlases()) {
        atlasMaterials.push_back(s.materials.load(atlas.image));
    }
    registerLevelEntities();
#ifndef NDEBUG
    // Every registered type and attribute, for prefab validation and future editors.
    const std::filesystem::path schemaPath = sourceAssetPath("assets/prefabs/prefab_schema.json");
    std::filesystem::create_directories(schemaPath.parent_path());
    std::ofstream(schemaPath) << exportSchema().dump(2) << '\n';
#endif
    // Lights belong to their room: dropped when it unloads.
    world.onRoomUnloaded = [this](const std::string& roomIid) {
        std::erase_if(lightEdits, [&](const EditableLight& l) { return l.roomIid == roomIid; });
    };

    Camera2D& camera = s.camera;
    camera.size = 2.0f;
    camera.followEnabled = true;
    camera.followSmoothness = 6.0f;
    camera.snapDistance = 0.001f;
    camera.boundsEnabled = true;

    loadScene(s, SceneId::Showcase);
    levelFiles.watch(sourceAssetPath("assets/levels/showcase.ldtk"), sourceAssetPath("assets/regions"), ".region.json");
    prefabFiles.watch(sourceAssetPath("assets/levels/showcase.ldtk"), prefabs.dir(), ".prefab.json"); // only .dir is used
    effectFiles.watch(sourceAssetPath("assets/levels/showcase.ldtk"), sourceAssetPath("assets/effects"), ".effect.json"); // only .dir is used

    // Debug (MUSH2D_CAMERA_X=<world x>): park the camera there, e.g. for parallax screenshots.
    if (const char* camX = std::getenv("MUSH2D_CAMERA_X")) {
        camera.followEnabled = false;
        camera.position.x = static_cast<float>(std::atof(camX));
    }
}
void IkigaiGame::update(EngineServices& s, float deltaTime) {
    Scene2D& scene = s.scene;

    handleGlobalInput(s, deltaTime);
#ifndef NDEBUG
    if (effectFiles.poll(deltaTime).dir) effects.scan(sourceAssetPath("assets/effects"));
    if (currentSceneId == SceneId::Showcase) {
        if (const FileWatcher::Change change = levelFiles.poll(deltaTime)) hotReload(s, !change.dir);
        // A saved prefab: reload the rooms like F5 (unsaved inspector edits are lost; the WorldState stays).
        if (prefabFiles.poll(deltaTime).dir) {
            prefabs.clear();
            hotReload(s, true);
        }
    }
#endif
    // The inspector's pause holds only while the F1 panel is open.
    const bool paused = inspectorPause && panelShown;
    panelShown = false;
    if (paused) {
        submitRenderData(s);
        return;
    }
    auto* player = dynamic_cast<IPlayer*>(livePlayer(scene, playerObjectId));
    // A death fade only exists in the Showcase world; leaving it mid-fade must not freeze other scenes.
    const bool dying = currentSceneId == SceneId::Showcase && world.dying();
    if (player && !dying) { // a dying player stands still
        player->applyInput(s.input.getState());
    }

    scene.step(deltaTime);

    if (activeSceneDefinition) {
        activeSceneDefinition->update(scene, deltaTime);
    }

    if (!dying) processTriggerOverlaps(s);
    scene.lateStep(deltaTime);

    if (currentSceneId == SceneId::Showcase) {
        world.state.clock += deltaTime;
        if (const GameObject* p = scene.findObject(playerObjectId)) {
            world.update(p->transform.position);
            const glm::vec2 half = s.camera.halfExtent(s.gpu.swapchain().extent());
            world.checkRespawns(p->transform.position,
                                glm::vec4(s.camera.position - half, s.camera.position + half), world.state.clock);
        }
        regions.applyNewLayerOverrides(levelLoader);
        s.renderer.settings() = regions.update(s.renderer.settings(), deltaTime);
        s.renderer.settings().fade = world.stepFade(deltaTime); // after regions.update, which rewrites settings
    }

    submitRenderData(s);
}

std::pair<Material, glm::vec4> IkigaiGame::lookupAtlasFrame(std::uint32_t atlasIndex, const std::string& frameName) const {
    if (atlasIndex >= atlasMaterials.size()) {
        return {};
    }
    const TextureAtlas& atlas = gameAssets.getAtlasByTextureIndex(atlasIndex);
    const AtlasMeta& meta = atlas.getMeta();
    if (!atlas.hasFrame(frameName) || meta.imageWidth <= 0 || meta.imageHeight <= 0) {
        return {};
    }
    const AtlasFrame& frame = atlas.getFrame(frameName);
    float w = static_cast<float>(meta.imageWidth);
    float h = static_cast<float>(meta.imageHeight);
    return {atlasMaterials[atlasIndex],
            glm::vec4(static_cast<float>(frame.x) / w, static_cast<float>(frame.y) / h,
                      static_cast<float>(frame.w) / w, static_cast<float>(frame.h) / h)};
}

void IkigaiGame::submitRenderData(EngineServices& s) {
    SpriteBatch& batch = s.renderer.sprites();
    batch.clear();
    levelLoader.appendStatic(batch);
    if (currentSceneId == SceneId::Showcase) {
        regions.submit(s.camera, s.gpu.swapchain().extent(), batch);
    }
    appendSceneSprites(s.scene, batch, [this](std::uint32_t atlasIndex, const std::string& frameName) {
        return lookupAtlasFrame(atlasIndex, frameName);
    }, &s.materials.white());

    // Debug (MUSH2D_DEBUG_ADDITIVE=1): bright additive sprite next to the player, to check additive + bloom.
    static const bool debugAdditive = std::getenv("MUSH2D_DEBUG_ADDITIVE") != nullptr;
    if (debugAdditive) {
        if (const GameObject* player = s.scene.findObject(playerObjectId)) {
            auto [material, uvRect] = lookupAtlasFrame(2, "teleport_0"); // environment atlas
            if (uvRect.z > 0.0f) {
                SpriteInstance inst{};
                inst.position = player->transform.position + glm::vec2(1.0f, 0.3f);
                inst.size = {0.6f, 0.6f};
                inst.uvRect = uvRect;
                inst.color = {4.0f, 2.5f, 1.0f, 1.0f};
                inst.parallax = {1.0f, 1.0f};
                inst.albedo = material.albedo; inst.normal = material.normal;
                inst.emissive = material.emissive; inst.mask = material.mask;
                inst.flags = SpriteFlagNearest | SpriteFlagAdditive;
                batch.add(inst);
            }
        }
    }

    LightList& lights = s.renderer.lights();
    lights.clear();
    if (currentSceneId == SceneId::Showcase) {
        submitShowcaseExtras(s);
        collectComponentLights(s.scene, lights);
        return;
    }

    // Test light: warm, slightly flickering, above and in front of the player.
    if (const GameObject* player = s.scene.findObject(playerObjectId)) {
        glm::vec2 p = player->transform.position + glm::vec2(0.6f, 0.8f);
        lights.add(GpuLight{{p.x, p.y, 1.0f, 5.0f}, {1.0f, 0.8f, 0.55f, 2.5f}, {0.0f, 0.08f, 7.0f, 0.0f}});
    }
    collectComponentLights(s.scene, lights);
}
void IkigaiGame::shutdown(EngineServices& s) {
    diedSubscription.reset();
    s.scene.clear();
    s.scene.setService<GameAssets>(nullptr);
    s.scene.setService<AssetRegistry>(nullptr);
    s.scene.setService<PrefabLibrary>(nullptr);
    s.scene.setService<EffectLibrary>(nullptr);
    s.scene.setService<World>(nullptr);
    s.scene.setService<WorldState>(nullptr);
}

bool IkigaiGame::tryLoadScene(EngineServices& s, SceneId sceneId, const SceneTransitionData& transitionData) {
    try {
        loadScene(s, sceneId, transitionData);
        return true;
    } catch (const std::exception& e) { // bad level file: nothing was torn down yet, keep the current scene
        std::fprintf(stderr, "Scene load failed, keeping current scene: %s\n", e.what());
        return false;
    }
}

void IkigaiGame::hotReload(EngineServices& s, bool ldtkOnly) {
    keepRegions = ldtkOnly; // only the .ldtk changed: keep the panel's unsaved look
    const bool wasDying = world.dying();
    const GameObject* old = s.scene.findObject(playerObjectId);
    const bool hadPlayer = old && old->hasTag("player");
    const glm::vec2 at = hadPlayer ? old->transform.position : glm::vec2(0.0f);
    std::fprintf(stderr, "Hot reload: level files changed\n");
    const bool loaded = tryLoadScene(s, SceneId::Showcase, currentSceneTransitionData);
    keepRegions = false;
    if (!loaded || !hadPlayer) return;
    GameObject* player = s.scene.findObject(playerObjectId);
    auto* control = dynamic_cast<IPlayer*>(player);
    if (!control || !player->collider) return;

    // Spec decision 8: same place, pushed out of new geometry up to 2 u, else the checkpoint.
    if (const int room = world.roomAt(at); room >= 0) world.loadAround(room);
    s.scene.flush(); // doors spawned by the reload take part in the query
    Body body;
    body.position = at + player->collider->offset;
    body.halfSize = player->collider->size * 0.5f;
    const glm::vec2 reach = body.halfSize + 3.0f;
    const CollisionQuery query{&s.scene.collision,
                               Collision2D::solidBoxes(s.scene, player->id, {body.position - reach, body.position + reach})};
    glm::vec2 target = respawnPoint();
    if (!wasDying && depenetrate(body, query)) target = body.position - player->collider->offset;
    else if (const int room = world.roomAt(target); room >= 0) world.loadAround(room);
    control->respawnAt(target);
    s.camera.position = target + s.camera.followOffset;
}

void IkigaiGame::loadScene(EngineServices& s, SceneId sceneId, const SceneTransitionData& transitionData) {
    // Parse before touching any state, so a malformed level throws with the old scene intact.
    if (sceneId == SceneId::Showcase) {
        preparedProject = loadLdtkProject(sourceAssetPath("assets/levels/showcase.ldtk"));
    }
    Camera2D& camera = s.camera;
    Scene2D& scene = s.scene;
    std::string previousSceneName;

    if (activeSceneDefinition) {
        previousSceneName = activeSceneDefinition->getSceneName();
        activeSceneDefinition->onExit(scene);
    }

    currentSceneId = sceneId;
    activeSceneDefinition = SceneRegistry::create(sceneId);

    currentSceneTransitionData = transitionData;
    if (currentSceneTransitionData.previousSceneName.empty()) {
        currentSceneTransitionData.previousSceneName = previousSceneName;
    }

    setupInitialScene(s);

    if (activeSceneDefinition) {
        activeSceneDefinition->onEnter(scene, currentSceneTransitionData);
    }

    GameObject* player = findPlayer(scene);
    if (player) {
        playerObjectId = player->id;
        camera.position = player->transform.position;
        camera.targetId = playerObjectId;
    } else {
        playerObjectId = InvalidGameObjectId;
        camera.targetId = InvalidGameObjectId;
    }

    activeTriggerOverlaps.clear();
    std::fprintf(stderr, "Scene: %s\n", activeSceneDefinition ? activeSceneDefinition->getSceneName() : "Showcase");
}

void IkigaiGame::reloadCurrentScene(EngineServices& s) {
    Scene2D& scene = s.scene;
    if (activeSceneDefinition) {
        activeSceneDefinition->onReload(scene);
    }
    tryLoadScene(s, currentSceneId, currentSceneTransitionData);
}

void IkigaiGame::handleGlobalInput(EngineServices& s, float deltaTime) {
    Scene2D& scene = s.scene;
    GlfwInput& input = s.input;
    Camera2D& camera = s.camera;
    if (input.isKeyDown(GLFW_KEY_TAB)) {
        if (!nextScenePressed) {
            SceneId nextScene =
                currentSceneId == SceneId::Showcase ? SceneId::Sandbox
                : currentSceneId == SceneId::Sandbox ? SceneId::Test
                : SceneId::Showcase;

            SceneTransitionData transitionData{};
            transitionData.reason = "TabSceneSwitch";

            tryLoadScene(s, nextScene, transitionData);
            nextScenePressed = true;
        }
    } else {
        nextScenePressed = false;
    }

    if (input.isKeyDown(GLFW_KEY_F5)) {
        if (!reloadScenePressed) {
            reloadCurrentScene(s);
            reloadScenePressed = true;
        }
    } else {
        reloadScenePressed = false;
    }

    const InputState& state = input.getState();


    if (state.toggleFollow.pressed) {
        camera.followEnabled = !camera.followEnabled;
    }

    GameObject* player = livePlayer(scene, playerObjectId);
    auto* control = dynamic_cast<IPlayer*>(player);

    if (state.respawn.pressed && player && currentSceneId == SceneId::Showcase) {
        killPlayer(s, player->id); // through World: restore, stream, fade
    } else if (state.respawn.pressed && control) {
        control->respawnAtCheckpoint();

        if (camera.followEnabled) {
            camera.position = player->transform.position + camera.followOffset;
        }
    }

#ifndef NDEBUG
    // Debug J = 1 damage, pushed away from where the Iki faces; L = spend 30 ice and 30 fire (spec D2 "Recursos").
    const bool hurtKey = input.isKeyDown(GLFW_KEY_J), spendKey = input.isKeyDown(GLFW_KEY_L);
    if (auto* iki = dynamic_cast<IkiObject*>(player); iki && hurtKey && !hurtPressed) iki->hurt({1, {-iki->facing, 0.0f}});
    if (Resources* resources = player ? player->getComponent<Resources>() : nullptr; resources && spendKey && !spendPressed)
        resources->spendBoth(30.0f, 30.0f);
    hurtPressed = hurtKey;
    spendPressed = spendKey;

    // Debug K = die: the room goes back to its entry state, respawn at the latest activated checkpoint.
    if (input.isKeyDown(GLFW_KEY_K)) {
        if (!killPressed && player && currentSceneId == SceneId::Showcase) killPlayer(s, player->id);
        killPressed = true;
    } else {
        killPressed = false;
    }
#endif

    if (state.scrollDeltaY != 0.0) {
        camera.size -= static_cast<float>(state.scrollDeltaY) * mouseZoomSensitivity * camera.size;
    }

    if (camera.followEnabled) {
        if (state.zoomOut.down) {
            camera.size += cameraZoomSpeed * deltaTime;
        }
        if (state.zoomIn.down) {
            camera.size -= cameraZoomSpeed * deltaTime;
        }

        camera.clampSize(s.gpu.swapchain().extent());
        return;
    }

    float moveAmount = cameraMoveSpeed * deltaTime;
    float zoomAmount = cameraZoomSpeed * deltaTime;

    if (state.cameraLeft.down) {
        camera.position.x -= moveAmount;
    }
    if (state.cameraRight.down) {
        camera.position.x += moveAmount;
    }
    if (state.cameraUp.down) {
        camera.position.y += moveAmount;
    }
    if (state.cameraDown.down) {
        camera.position.y -= moveAmount;
    }

    if (state.zoomOut.down) {
        camera.size += zoomAmount;
    }
    if (state.zoomIn.down) {
        camera.size -= zoomAmount;
    }

    if (state.dragCamera.down) {
        glm::vec2 half = camera.halfExtent(s.gpu.swapchain().extent());
        float worldHeight = half.y * 2.0f;
        float worldWidth = half.x * 2.0f;

        float unitsPerPixelX = worldWidth / static_cast<float>(s.gpu.swapchain().extent().width);
        float unitsPerPixelY = worldHeight / static_cast<float>(s.gpu.swapchain().extent().height);

        camera.position.x -= static_cast<float>(state.mouseDeltaX) * unitsPerPixelX;
        camera.position.y += static_cast<float>(state.mouseDeltaY) * unitsPerPixelY;
    }

    camera.clampSize(s.gpu.swapchain().extent());
}

void IkigaiGame::processTriggerOverlaps(EngineServices& s) {
    Scene2D& scene = s.scene;
    GameObject* player = scene.findObject(playerObjectId);
    if (!player || !player->active || !player->collider.has_value() || !player->collider->enabled) {
        return;
    }

    Collision2D::AABB playerBox = Collision2D::buildAABB(*player);
    std::unordered_set<GameObjectId> currentOverlaps;

    for (GameObjectId id : Collision2D::queryAABB(scene, playerBox, Collision2D::QueryKind::Trigger)) {
        GameObject* object = scene.findObject(id);
        // Handlers may destroy or edit objects; re-check live state. Triggers are static, so the grid box is current.
        if (!object || !object->active || id == playerObjectId || !Collision2D::hasCollider(*object) ||
            !object->collider->isTrigger || !Collision2D::intersects(playerBox, Collision2D::buildAABB(*object))) {
            continue;
        }

        currentOverlaps.insert(id);

        if (activeTriggerOverlaps.find(id) == activeTriggerOverlaps.end()) {
            object->onTriggerEnter(scene, *player);
            player->onTriggerEnter(scene, *object);
        }
    }

    for (GameObjectId previousId : activeTriggerOverlaps) {
        if (currentOverlaps.find(previousId) == currentOverlaps.end()) {
            GameObject* previous = scene.findObject(previousId);
            if (previous) {
                previous->onTriggerExit(scene, *player);
                player->onTriggerExit(scene, *previous);
            }
        }
    }

    activeTriggerOverlaps = std::move(currentOverlaps);
}

void IkigaiGame::setupInitialScene(EngineServices& s) {
    Scene2D& scene = s.scene;
    activeTriggerOverlaps.clear();
    levelLoader.clear();
    lightEdits.clear();
    s.renderer.settings() = FrameSettings{};
    s.camera.bounds = Camera2D::WorldBounds{-5.0f, 5.0f, -3.0f, 3.0f};
    s.camera.followOffset = {0.0f, 0.0f};

    if (currentSceneId == SceneId::Showcase) {
        loadShowcaseLevel(s);
        return;
    }

    if (!activeSceneDefinition) {
        throw std::runtime_error("Nenhuma definicao de cena ativa foi configurada.");
    }

    ISceneDefinition::BuildResult result =
        activeSceneDefinition->build(scene, currentSceneTransitionData);

    playerObjectId = result.playerObjectId;
}

// Only the special entities have a code factory. Every other entity (Slime, Door, Lever, Item, Checkpoint,
// Critter, Prop, Crystal, ...) creates assets/prefabs/<snake_case name>.prefab.json (LevelLoader::spawnEntity).
void IkigaiGame::registerLevelEntities() {
    // The Iki (assets/prefabs/iki.prefab.json). Its position is its feet, on the entity's base.
    levelLoader.registerEntity("PlayerStart", [this](const LdtkEntity& e, glm::vec2 pos, EngineServices& s) {
        if (findPlayer(s.scene)) return; // its room streamed in again
        GameObject* iki = prefabs.instantiate(s.scene, "iki");
        if (!iki) return; // the prefab library printed why
        iki->transform.position = bottomOf(e, pos);
        iki->persistent = true; // the player never belongs to a room
        iki->roomIid.clear();
        if (auto* player = dynamic_cast<IPlayer*>(iki)) player->setCheckpoint(iki->transform.position);
    });

    // Lights are not scene objects: kept per room (roomIid) and dropped when it unloads (world.onRoomUnloaded).
    // A light's values come from its room's region file when it overrides that iid, else from LDtk.
    levelLoader.registerEntity("Light", [this](const LdtkEntity& e, glm::vec2 pos, EngineServices& s) {
        const std::string& room = s.scene.spawnRoomIid;
        // Death reloads its room within one frame: the light is still listed (keep it and its live edits).
        if (std::any_of(lightEdits.begin(), lightEdits.end(), [&](const EditableLight& l) { return l.iid == e.iid; })) return;
        glm::vec3 color = colorField(e, "color", glm::vec3(1.0f, 0.76f, 0.48f));
        LightOverride value{color, numberField(e, "intensity", 2.0f), numberField(e, "radius", 6.0f),
                            numberField(e, "height", 1.5f), numberField(e, "depth", 0.0f), numberField(e, "flicker", 0.0f)};
        const auto& overrides = regions.ofRoom(room).look.lights;
        if (auto it = overrides.find(e.iid); it != overrides.end()) value = it->second;
        lightEdits.push_back({e.iid, pos, value, room});
    });
}

void IkigaiGame::loadShowcaseLevel(EngineServices& s) {
    s.scene.clear();
    world.open(*preparedProject, levelLoader, &s);
    preparedProject.reset();

    // Start in the room holding PlayerStart (the first room if none does).
    const std::vector<LdtkLevel>& levels = world.project().levels;
    int start = 0;
    for (int i = 0; i < int(levels.size()); ++i)
        for (const LdtkLayer& L : levels[i].layers)
            for (const LdtkEntity& e : L.entities)
                if (e.identifier == "PlayerStart") start = i;
    regions.open(world, start, keepRegions);
    if (!levels.empty()) world.loadAround(start);

    const glm::vec4 b = world.worldBounds(); // (minX, minY, maxX, maxY)
    s.camera.bounds = Camera2D::WorldBounds{b.x, b.z, b.y, b.w};
    s.camera.followOffset = {0.0f, 1.4f}; // the Iki's position is its feet

    regions.applyNewLayerOverrides(levelLoader); // lights got theirs per room, in the Light factory
    s.renderer.settings() = regions.current().look.settings;
}

void IkigaiGame::killPlayer(EngineServices& s, GameObjectId id) {
    if (world.dying()) return;
    if (auto* p = s.scene.findObject(id)) p->active = false; // frozen until the teleport
    world.killPlayer(respawnPoint(), [&scene = s.scene, &camera = s.camera, id](glm::vec2 at) {
        GameObject* p = scene.findObject(id);
        if (auto* player = dynamic_cast<IPlayer*>(p)) {
            p->active = true;
            player->respawnAt(at);
            if (auto* iki = dynamic_cast<IkiObject*>(p)) iki->respawn(); // full health and energy, state idle
            camera.position = at + camera.followOffset;
        }
    });
}

// Where the player stands after dying: the latest activated Checkpoint ("activated" = activation time), else
// PlayerStart. Same placement as the PlayerStart factory: the Iki's feet (its position) on the entity's base.
glm::vec2 IkigaiGame::respawnPoint() const {
    const LdtkLevel* bestLevel = nullptr;
    const LdtkEntity* best = nullptr;
    double bestTime = -1.0;
    for (const LdtkLevel& l : world.project().levels)
        for (const LdtkLayer& L : l.layers)
            for (const LdtkEntity& e : L.entities) {
                const double t = e.identifier == "Checkpoint" ? world.state.get(e.iid, "activated", -1.0)
                               : e.identifier == "PlayerStart" ? -0.5 : -1.0; // PlayerStart only beats nothing
                if (t > bestTime) { bestTime = t; best = &e; bestLevel = &l; }
            }
    if (!best) return glm::vec2(0.0f);
    const glm::vec2 centre = ldtkToWorld(bestLevel->worldPx, best->px + best->size * 0.5f);
    return bottomOf(*best, centre);
}

void IkigaiGame::onHud(EngineServices& s) {
    if (const GameObject* player = s.scene.findObject(playerObjectId))
        if (const auto* resources = player->getComponent<Resources>()) drawDebugHud(*resources);
}

void IkigaiGame::onImGui(EngineServices& s) {
    panelShown = true;
    inspector.draw(s.scene, prefabs, inspectorPause);
    // Selection outline: the collider box (else the scale), drawn under the ImGui windows.
    if (const GameObject* o = s.scene.findObject(inspector.selected())) {
        const Transform2D w = o->world();
        const glm::vec2 centre = w.position + (o->collider ? o->collider->offset : glm::vec2(0.0f));
        const glm::vec2 half = o->collider ? o->collider->size * 0.5f : glm::abs(w.scale) * 0.5f;
        const glm::vec2 view = s.camera.halfExtent(s.gpu.swapchain().extent());
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        // shortcut: ignores the sprite's parallax, so objects on parallax layers are outlined off their place.
        auto toScreen = [&](glm::vec2 p) {
            const glm::vec2 n = (p - s.camera.position) / view;
            return ImVec2((n.x + 1.0f) * 0.5f * display.x, (1.0f - n.y) * 0.5f * display.y);
        };
        ImGui::GetBackgroundDrawList()->AddRect(toScreen({centre.x - half.x, centre.y + half.y}),
                                                toScreen({centre.x + half.x, centre.y - half.y}), IM_COL32(255, 220, 0, 255), 0.0f, 0, 2.0f);
    }
    if (currentSceneId != SceneId::Showcase) return;
    // The panel shows the current room's tile layers (they belong to its region, regions.current = room.region).
    std::vector<EditableLayer> roomLayers;
    if (world.currentRoom() >= 0) {
        const LdtkLevel& room = world.project().levels[world.currentRoom()];
        for (const LevelLoader::LayerState& l : levelLoader.layers()) {
            const bool inRoom = std::any_of(room.layers.begin(), room.layers.end(),
                                            [&](const LdtkLayer& L) { return L.identifier == l.identifier; });
            if (l.region == regions.currentName() && inRoom) roomLayers.push_back({l.identifier, {l.parallax, l.depth}});
        }
    }
    bool save = false;
    lookPanel.draw(regions.current(), lightEdits, roomLayers, s.renderer.timers(), save);
    // Look edits reach the renderer through regions.update next frame (it may be blending).
    for (const EditableLayer& l : roomLayers) levelLoader.setLayer(regions.currentName(), l.identifier, l.value.parallax, l.value.depth);
    if (save) {
        regions.save(levelLoader, lightEdits);
        levelFiles.sync(); // our own save is not an outside edit
    }
}

void IkigaiGame::submitShowcaseExtras(EngineServices& s) {
    LightList& lights = s.renderer.lights();
    for (const EditableLight& l : lightEdits) {
        const LightOverride& v = l.value;
        lights.add(GpuLight{{l.pos.x, l.pos.y, v.height, v.radius}, {v.color, v.intensity}, {v.depth, v.flicker, 5.0f, 0.0f}});
    }
}
