#include "game/ShowcaseGame.hpp"

#include "engine/core/Paths.hpp"
#include "engine/world/Collision.hpp"
#include "game/objects/PlayerObject.hpp"
#include "game/scenes/SceneRegistry.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

void ShowcaseGame::init(EngineServices& s) {
    gameAssets.registerDefaults();
    s.scene.userData = &gameAssets;

    // Index order = GameAtlasId (Characters, Enemies, Environment).
    atlasMaterials.clear();
    for (const char* name : {"characters", "enemies", "environment"}) {
        atlasMaterials.push_back(s.materials.load(assetPath(std::string("assets/atlases/") + name + ".png")));
    }

    loadScene(s, SceneId::Sandbox);

    Camera2D& camera = s.camera;
    auto* player = s.scene.findFirstObjectOfType<PlayerObject>();
    if (player) {
        camera.position = player->transform.position;
        playerObjectId = player->id;
    }

    camera.size = 2.0f;
    camera.followEnabled = true;
    camera.targetId = playerObjectId;
    camera.followOffset = {0.0f, 0.0f};
    camera.followSmoothness = 6.0f;
    camera.snapDistance = 0.001f;
    camera.boundsEnabled = true;
    camera.bounds = Camera2D::WorldBounds{
        -5.0f,
         5.0f,
        -3.0f,
         3.0f
    };
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
    appendSceneSprites(s.scene, batch, [this](std::uint32_t atlasIndex, const std::string& frameName) {
        return lookupAtlasFrame(atlasIndex, frameName);
    });

    // Test light: warm, slightly flickering, above and in front of the player.
    LightList& lights = s.renderer.lights();
    lights.clear();
    if (const GameObject* player = s.scene.findObject(playerObjectId)) {
        glm::vec2 p = player->transform.position + glm::vec2(0.6f, 0.8f);
        lights.add(GpuLight{{p.x, p.y, 1.0f, 5.0f}, {1.0f, 0.8f, 0.55f, 2.5f}, {0.0f, 0.08f, 7.0f, 0.0f}});
    }
}

void ShowcaseGame::shutdown(EngineServices& s) {
    s.scene.clear();
    s.scene.userData = nullptr;
}

void ShowcaseGame::loadScene(EngineServices& s, SceneId sceneId, const SceneTransitionData& transitionData) {
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
}

void ShowcaseGame::reloadCurrentScene(EngineServices& s) {
    Scene2D& scene = s.scene;
    if (!activeSceneDefinition) {
        throw std::runtime_error("Nao existe cena ativa para recarregar.");
    }

    activeSceneDefinition->onReload(scene);
    loadScene(s, currentSceneId, currentSceneTransitionData);
}

void ShowcaseGame::handleGlobalInput(EngineServices& s, float deltaTime) {
    Scene2D& scene = s.scene;
    GlfwInput& input = s.input;
    Camera2D& camera = s.camera;
    if (input.isKeyDown(GLFW_KEY_TAB)) {
        if (!nextScenePressed) {
            SceneId nextScene =
                (currentSceneId == SceneId::Sandbox)
                ? SceneId::Test
                : SceneId::Sandbox;

            SceneTransitionData transitionData{};
            transitionData.reason = "TabSceneSwitch";

            // auto* player = scene.findObjectAs<PlayerObject>(playerObjectId);
            // if (!player) {
            //     player = scene.findFirstObjectOfType<PlayerObject>();
            // }

            // if (player) {
            //     transitionData.playerSpawnPosition = player->transform.position;
            //     transitionData.overridePlayerSpawn = true;
            //     transitionData.preferredPlayerObjectId = player->id;
            // }

            loadScene(s, nextScene, transitionData);
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

    for (auto& objectPtr : scene.getObjects()) {
        if (!objectPtr || !objectPtr->active) {
            continue;
        }

        if (objectPtr->id == playerObjectId) {
            continue;
        }

        if (!objectPtr->collider.has_value() || !objectPtr->collider->enabled) {
            continue;
        }

        if (!objectPtr->collider->isTrigger) {
            continue;
        }

        Collision2D::AABB triggerBox = Collision2D::buildAABB(*objectPtr);

        if (Collision2D::intersects(playerBox, triggerBox)) {
            currentOverlaps.insert(objectPtr->id);

            if (activeTriggerOverlaps.find(objectPtr->id) == activeTriggerOverlaps.end()) {
                objectPtr->onTriggerEnter(scene, *player);
                player->onTriggerEnter(scene, *objectPtr);
            }
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

    if (!activeSceneDefinition) {
        throw std::runtime_error("Nenhuma definicao de cena ativa foi configurada.");
    }

    ISceneDefinition::BuildResult result =
        activeSceneDefinition->build(scene, currentSceneTransitionData);

    playerObjectId = result.playerObjectId;
}

