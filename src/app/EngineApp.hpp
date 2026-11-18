#pragma once

#include "game/objects/CheckpointObject.hpp"
#include "game/objects/PlayerObject.hpp"
#include "game/objects/SlimeEnemy.hpp"
#include "game/objects/TeleportObject.hpp"
#include "game/objects/WallObject.hpp"
#include "engine/gpu/GpuContext.hpp"
#include "engine/graphics/Camera2D.hpp"
#include "engine/graphics/Renderer2D.hpp"
#include "engine/platform/GlfwInput.hpp"
#include "engine/world/Collision.hpp"
#include "engine/world/Scene2D.hpp"
#include "game/spawners/Spawn.hpp"
#include "engine/scenes/ISceneDefinition.hpp"
#include "game/scenes/SandboxScene.hpp"
#include "game/scenes/TestScene.hpp"
#include "engine/scenes/SceneRegistry.hpp"
#include "engine/scenes/SceneId.hpp"
#include "game/assets/GameAssets.hpp"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>
#include <memory>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

class EngineApp {
public:
    void run();

private:
    static constexpr uint32_t WIDTH = 1280;
    static constexpr uint32_t HEIGHT = 720;

private:
    GLFWwindow* window = nullptr;
    GpuContext gpu;

    // Task 2: Renderer2D is not initialised (its pipeline needs a VkRenderPass); frames are
    // clear-only until Task 5 replaces it.
    Renderer2D renderer2D;
    Camera2D camera{};
    GlfwInput input;

    Scene2D scene{};

    GameObjectId playerObjectId = InvalidGameObjectId;

    float mouseZoomSensitivity = 0.15f;
    float cameraMoveSpeed = 1.5f;
    float cameraZoomSpeed = 1.0f;

    std::unordered_set<GameObjectId> activeTriggerOverlaps;

    std::unique_ptr<ISceneDefinition> activeSceneDefinition;

    SceneId currentSceneId = SceneId::Sandbox;
    bool nextScenePressed = false;

    bool reloadScenePressed = false;

    SceneTransitionData pendingTransitionData{};
    SceneTransitionData currentSceneTransitionData{};

    GameAssets gameAssets;

private:
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();

    void drawFrame();

    void handleGlobalInput(float deltaTime);
    void processTriggerOverlaps();
    void setupInitialScene();

    void reloadCurrentScene();

    void loadScene(SceneId sceneId, const SceneTransitionData& transitionData = {});
};
