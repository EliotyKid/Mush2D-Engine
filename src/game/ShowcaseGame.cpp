#include "game/ShowcaseGame.hpp"

#include "engine/core/Paths.hpp"
#include "engine/render/LightComponent.hpp"
#include "engine/world/Collision.hpp"
#include "game/assets/GameAtlasId.hpp"
#include "game/objects/CritterObject.hpp"
#include "game/objects/PivotSprite.hpp"
#include "game/objects/PlayerObject.hpp"
#include "game/scenes/SceneRegistry.hpp"
#include "game/spawners/Spawn.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

void ShowcaseGame::init(EngineServices& s) {
    gameAssets.registerDefaults();
    s.scene.userData = &gameAssets;

    atlasMaterials.clear();
    for (const char* path : GameAtlasId::ImagePaths) {
        atlasMaterials.push_back(s.materials.load(assetPath(path)));
    }
    registerLevelEntities();

    Camera2D& camera = s.camera;
    camera.size = 2.0f;
    camera.followEnabled = true;
    camera.followSmoothness = 6.0f;
    camera.snapDistance = 0.001f;
    camera.boundsEnabled = true;

    loadScene(s, SceneId::Showcase);

    // Debug (MUSH2D_CAMERA_X=<world x>): park the camera there, e.g. for parallax screenshots.
    if (const char* camX = std::getenv("MUSH2D_CAMERA_X")) {
        camera.followEnabled = false;
        camera.position.x = static_cast<float>(std::atof(camX));
    }
}
void ShowcaseGame::update(EngineServices& s, float deltaTime) {
    Scene2D& scene = s.scene;

    handleGlobalInput(s, deltaTime);
    auto* player = scene.findObjectAs<PlayerObject>(playerObjectId);
    if (!player) {
        player = scene.findFirstObjectOfType<PlayerObject>();
        if (player) {
            playerObjectId = player->id;
        }
    }
    if (player) {
        player->applyInput(s.input.getState());
    }

    scene.step(deltaTime);

    if (activeSceneDefinition) {
        activeSceneDefinition->update(scene, deltaTime);
    }

    processTriggerOverlaps(s);
    scene.lateStep(deltaTime);

    submitRenderData(s);
}

std::pair<Material, glm::vec4> ShowcaseGame::lookupAtlasFrame(std::uint32_t atlasIndex, const std::string& frameName) const {
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

void ShowcaseGame::submitRenderData(EngineServices& s) {
    SpriteBatch& batch = s.renderer.sprites();
    batch.clear();
    levelLoader.appendStatic(batch);
    appendSceneSprites(s.scene, batch, [this](std::uint32_t atlasIndex, const std::string& frameName) {
        return lookupAtlasFrame(atlasIndex, frameName);
    });

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
void ShowcaseGame::shutdown(EngineServices& s) {
    s.scene.clear();
    s.scene.userData = nullptr;
}

void ShowcaseGame::tryLoadScene(EngineServices& s, SceneId sceneId, const SceneTransitionData& transitionData) {
    try {
        loadScene(s, sceneId, transitionData);
    } catch (const std::exception& e) { // bad level file: nothing was torn down yet, keep the current scene
        std::fprintf(stderr, "Scene load failed, keeping current scene: %s\n", e.what());
    }
}

void ShowcaseGame::loadScene(EngineServices& s, SceneId sceneId, const SceneTransitionData& transitionData) {
    // Parse before touching any state, so a malformed level throws with the old scene intact.
    if (sceneId == SceneId::Showcase) {
        preparedLevel = loadLdtkLevel(sourceAssetPath("assets/levels/showcase.ldtk"), "Showcase");
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

    auto* player = scene.findFirstObjectOfType<PlayerObject>();
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

void ShowcaseGame::reloadCurrentScene(EngineServices& s) {
    Scene2D& scene = s.scene;
    if (activeSceneDefinition) {
        activeSceneDefinition->onReload(scene);
    }
    tryLoadScene(s, currentSceneId, currentSceneTransitionData);
}

void ShowcaseGame::handleGlobalInput(EngineServices& s, float deltaTime) {
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

    auto* player = scene.findObjectAs<PlayerObject>(playerObjectId);
    if (!player) {
        player = scene.findFirstObjectOfType<PlayerObject>();
        if (player) {
            playerObjectId = player->id;
        }
    }

    if (state.respawn.pressed && player) {
        player->respawnAtCheckpoint();

        if (camera.followEnabled) {
            camera.position = player->transform.position + camera.followOffset;
        }
    }

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

        camera.size = std::clamp(camera.size, 0.2f, 20.0f);
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

    camera.size = std::clamp(camera.size, 0.2f, 20.0f);
}

void ShowcaseGame::processTriggerOverlaps(EngineServices& s) {
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

void ShowcaseGame::setupInitialScene(EngineServices& s) {
    Scene2D& scene = s.scene;
    activeTriggerOverlaps.clear();
    levelLoader.clear();
    lightEdits.clear();
    layerEdits.clear();
    crystals.clear();
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

namespace {
constexpr glm::vec3 CrystalGlow[4] = {{0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.9f, 1.0f}, {1.0f, 0.16f, 0.86f}};

float numberField(const LdtkEntity& e, const char* key, float fallback) {
    auto it = e.fields.find(key);
    return it != e.fields.end() && it->is_number() ? it->get<float>() : fallback;
}

std::string stringField(const LdtkEntity& e, const char* key, const std::string& fallback) {
    auto it = e.fields.find(key);
    return it != e.fields.end() && it->is_string() ? it->get<std::string>() : fallback;
}

bool boolField(const LdtkEntity& e, const char* key) {
    auto it = e.fields.find(key);
    return it != e.fields.end() && it->is_boolean() && it->get<bool>();
}

// "#RRGGBB" -> linear-ish 0..1 (lights are HDR intensities, so plain /255 is fine).
glm::vec3 colorField(const LdtkEntity& e, const char* key, glm::vec3 fallback) {
    std::string hex = stringField(e, key, "");
    unsigned rgb = 0;
    if (hex.size() != 7 || hex[0] != '#' || std::sscanf(hex.c_str() + 1, "%6x", &rgb) != 1) return fallback;
    return glm::vec3((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF) / 255.0f;
}

// Entities use pivot (0.5, 1): the bottom-centre of the box is where they stand.
glm::vec2 bottomOf(const LdtkEntity& e, glm::vec2 centre) {
    return centre - glm::vec2(0.0f, e.size.y / (2.0f * LdtkPixelsPerUnit));
}
}

void ShowcaseGame::registerLevelEntities() {
    levelLoader.registerEntity("PlayerStart", [](const LdtkEntity& e, glm::vec2 pos, EngineServices& s) {
        PlayerObject::Config c{};
        c.movementMode = PlayerObject::Config::MovementMode::Platformer;
        c.moveSpeed = 4.5f;
        c.colliderSize = {0.6f, 1.6f};
        c.gravity = -32.0f;  // apex = 11^2 / 64 = 1.89 units = 3.8 tiles
        c.jumpSpeed = 11.0f;
        c.jumpCutFactor = 0.5f;
        c.maxFallSpeed = -18.0f;
        PlayerObject& player = Spawn::player(s.scene, bottomOf(e, pos) + glm::vec2(0.0f, 0.5f * c.colliderSize.y), c);
        player.setCheckpointPosition(player.transform.position);
    });

    levelLoader.registerEntity("Light", [this](const LdtkEntity& e, glm::vec2 pos, EngineServices&) {
        glm::vec3 color = colorField(e, "color", glm::vec3(1.0f, 0.76f, 0.48f));
        lightEdits.push_back({e.iid, pos,
            {color, numberField(e, "intensity", 2.0f), numberField(e, "radius", 6.0f), numberField(e, "height", 1.5f),
             numberField(e, "depth", 0.0f), numberField(e, "flicker", 0.0f)}});
    });

    levelLoader.registerEntity("Crystal", [this](const LdtkEntity& e, glm::vec2 pos, EngineServices&) {
        int variant = static_cast<int>(numberField(e, "variant", 0.0f));
        crystals.push_back({bottomOf(e, pos), std::clamp(variant, 0, 3)});
    });

    levelLoader.registerEntity("Prop", [this](const LdtkEntity& e, glm::vec2 pos, EngineServices& s) {
        std::string frameName = stringField(e, "frame", "");
        const TextureAtlas& atlas = gameAssets.getAtlasByTextureIndex(GameAtlasId::KaaProps);
        if (!atlas.hasFrame(frameName)) {
            std::fprintf(stderr, "Showcase: prop frame '%s' not found, ignored\n", frameName.c_str());
            return;
        }
        float depth = e.fields.contains("depth") ? numberField(e, "depth", e.depth) : e.depth;
        GameObject& prop = s.scene.createObject<GameObject>();
        prop.name = "Prop";
        prop.transform.position = bottomOf(e, pos);
        // Foreground props read as dark silhouettes, as in Ori.
        glm::vec4 color = depth < 0.0f ? glm::vec4(0.32f, 0.36f, 0.42f, 1.0f) : glm::vec4(1.0f);
        prop.sprite = SpriteData{.atlasTextureIndex = GameAtlasId::KaaProps, .frameName = frameName, .layer = 1,
                                 .color = color, .depth = depth, .parallax = e.parallax};
        placeFrameAtPivot(prop, atlas.getFrame(frameName), {0.0f, 0.0f}, boolField(e, "flipX"));
    });

    levelLoader.registerEntity("Critter", [](const LdtkEntity& e, glm::vec2 pos, EngineServices& s) {
        CritterObject& critter = s.scene.createObject<CritterObject>(
            GameAtlasId::KaaAnimals, stringField(e, "anim", "fox_idle"), boolField(e, "flipX"));
        critter.transform.position = bottomOf(e, pos);
    });
}

void ShowcaseGame::loadShowcaseLevel(EngineServices& s) {
    s.scene.clear();
    LdtkLevel level = std::move(*preparedLevel);
    preparedLevel.reset();
    levelLoader.load(level, s, s.materials);

    glm::vec2 size = glm::vec2(level.pxSize) / LdtkPixelsPerUnit;
    s.camera.bounds = Camera2D::WorldBounds{0.0f, size.x, 0.0f, size.y};
    s.camera.followOffset = {0.0f, 0.6f};

    // Look comes from showcase.look.json (Debug: the source-tree file, so panel saves survive rebuilds).
    for (const LevelLoader::LayerState& l : levelLoader.layers()) layerEdits.push_back({l.identifier, {l.parallax, l.depth}});
    look = loadLookOrDefault(sourceAssetPath("assets/levels/showcase.look.json"), FrameSettings{});
    applyLook(look, lightEdits, layerEdits);
    for (const EditableLayer& l : layerEdits) levelLoader.setLayer(l.identifier, l.value.parallax, l.value.depth);
    s.renderer.settings() = look.settings;
}

void ShowcaseGame::onImGui(EngineServices& s) {
    if (currentSceneId != SceneId::Showcase) return;
    bool save = false;
    lookPanel.draw(look, lightEdits, layerEdits, s.renderer.timers(), save);
    s.renderer.settings() = look.settings;
    for (const EditableLayer& l : layerEdits) levelLoader.setLayer(l.identifier, l.value.parallax, l.value.depth);
    if (save) {
        captureLook(look, lightEdits, layerEdits);
        saveLook(sourceAssetPath("assets/levels/showcase.look.json"), look);
    }
}

void ShowcaseGame::submitShowcaseExtras(EngineServices& s) {
    SpriteBatch& batch = s.renderer.sprites();
    LightList& lights = s.renderer.lights();
    for (const EditableLight& l : lightEdits) {
        const LightOverride& v = l.value;
        lights.add(GpuLight{{l.pos.x, l.pos.y, v.height, v.radius}, {v.color, v.intensity}, {v.depth, v.flicker, 5.0f, 0.0f}});
    }

    const glm::vec4 haloUv = lookupAtlasFrame(GameAtlasId::Crystals, "halo 0").second; // black in crystals_e
    const TextureAtlas& crystalAtlas = gameAssets.getAtlasByTextureIndex(GameAtlasId::Crystals);
    for (const Crystal& c : crystals) {
        const std::string frameName = "crystal " + std::to_string(c.variant);
        auto [material, uv] = lookupAtlasFrame(GameAtlasId::Crystals, frameName);
        if (uv.z <= 0.0f) continue;
        const glm::vec3 glow = CrystalGlow[c.variant];
        const AtlasFrame& f = crystalAtlas.getFrame(frameName); // pivot = base centre
        const glm::vec2 centre = c.anchor + glm::vec2(0.0f, float(f.pivotY) - 0.5f * float(f.h)) / ArtPixelsPerUnit;

        SpriteInstance inst{};
        inst.position = centre;
        inst.size = {1.0f, 1.0f};
        inst.uvRect = uv;
        inst.color = glm::vec4(1.0f);
        inst.depth = 0.1f; // just behind the player
        inst.parallax = {1.0f, 1.0f};
        inst.albedo = material.albedo; inst.normal = material.normal;
        inst.emissive = material.emissive; inst.mask = material.mask;
        inst.flags = SpriteFlagNearest;
        batch.add(inst);

        if (haloUv.z > 0.0f) {
            SpriteInstance halo = inst;
            halo.size = {2.8f, 2.8f};
            halo.uvRect = haloUv;
            halo.color = glm::vec4(glow * 0.45f, 1.0f);
            halo.depth = 0.0f;
            halo.flags = SpriteFlagAdditive; // LINEAR: soft
            batch.add(halo);
        }
        lights.add(GpuLight{{centre.x, centre.y, 0.6f, 3.0f}, {glow, 1.4f}, {0.0f, 0.06f, 2.0f, 0.0f}});
    }
}