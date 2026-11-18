#include "EngineApp.hpp"
#include "engine/core/Paths.hpp"
#include "engine/gpu/Image.hpp"
#include "engine/scenes/SceneRegistry.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

void EngineApp::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void EngineApp::initWindow() {
    if (!glfwInit()) {
        throw std::runtime_error("Erro: falha ao inicializar GLFW.");
    }

    if (!glfwVulkanSupported()) {
        glfwTerminate();
        throw std::runtime_error("Erro: Vulkan nao esta disponivel no sistema via GLFW.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(
        static_cast<int>(WIDTH),
        static_cast<int>(HEIGHT),
        "Mush2D Engine - Vulkan",
        nullptr,
        nullptr
    );

    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Erro: falha ao criar a janela.");
    }

    input.attachWindow(window);
}

void EngineApp::initVulkan() {
    gpu.init(window);

    gameAssets.registerDefaults();
    scene.gameAssets = &gameAssets;

    loadScene(SceneId::Sandbox);

    auto* player = scene.findFirstObjectOfType<PlayerObject>();
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

void EngineApp::mainLoop() {
    float lastTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        input.beginFrame();
        input.poll();

        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        handleGlobalInput(deltaTime);
        auto* player = scene.findObjectAs<PlayerObject>(playerObjectId);
        if (!player) {
            player = scene.findFirstObjectOfType<PlayerObject>();
            if (player) {
                playerObjectId = player->id;
            }
        }
        if (player) {
            player->applyInput(input.getState());
        }

        scene.step(deltaTime);

        if (activeSceneDefinition) {
            activeSceneDefinition->update(scene, deltaTime);
        }

        processTriggerOverlaps();
        scene.lateStep(deltaTime);

        camera.updateFollow(scene, deltaTime);
        camera.clampToBounds(gpu.swapchain().extent());

        drawFrame();
    }
}

void EngineApp::cleanup() {
    scene.clear();
    gpu.shutdown();

    if (window) {
        glfwDestroyWindow(window);
    }

    glfwTerminate();
}
void EngineApp::loadScene(SceneId sceneId, const SceneTransitionData& transitionData) {
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

    setupInitialScene();

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

void EngineApp::reloadCurrentScene() {
    if (!activeSceneDefinition) {
        throw std::runtime_error("Nao existe cena ativa para recarregar.");
    }

    activeSceneDefinition->onReload(scene);
    loadScene(currentSceneId, currentSceneTransitionData);
}

void EngineApp::handleGlobalInput(float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
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

            loadScene(nextScene, transitionData);
            nextScenePressed = true;
        }
    } else {
        nextScenePressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
        if (!reloadScenePressed) {
            reloadCurrentScene();
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
        float worldHeight = camera.size;
        float worldWidth = camera.size *
            (static_cast<float>(gpu.swapchain().extent().width) / static_cast<float>(gpu.swapchain().extent().height));

        float unitsPerPixelX = worldWidth / static_cast<float>(gpu.swapchain().extent().width);
        float unitsPerPixelY = worldHeight / static_cast<float>(gpu.swapchain().extent().height);

        camera.position.x -= static_cast<float>(state.mouseDeltaX) * unitsPerPixelX;
        camera.position.y += static_cast<float>(state.mouseDeltaY) * unitsPerPixelY;
    }

    camera.size = std::clamp(camera.size, 0.2f, 20.0f);
}

void EngineApp::processTriggerOverlaps() {
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

void EngineApp::setupInitialScene() {
    activeTriggerOverlaps.clear();

    if (!activeSceneDefinition) {
        throw std::runtime_error("Nenhuma definicao de cena ativa foi configurada.");
    }

    ISceneDefinition::BuildResult result =
        activeSceneDefinition->build(scene, currentSceneTransitionData);

    playerObjectId = result.playerObjectId;
}

void EngineApp::drawFrame() {
    FrameContext* frame = gpu.beginFrame();
    if (!frame) {
        return;
    }

    Swapchain& sc = gpu.swapchain();
    VkImage image = sc.image(frame->imageIndex);

    transition(frame->cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    color.imageView = sc.view(frame->imageIndex);
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color = {{0.05f, 0.05f, 0.08f, 1.0f}};

    VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering.renderArea = {{0, 0}, sc.extent()};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;

    vkCmdBeginRendering(frame->cmd, &rendering);
    // Sprites are not drawn until Task 5 (new renderer).
    vkCmdEndRendering(frame->cmd);

    transition(frame->cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    gpu.endFrame();
}
