#include "engine/core/Engine.hpp"
#include "engine/gpu/Image.hpp"

#include <algorithm>
#include <stdexcept>

namespace {
constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;
constexpr float MAX_DELTA_TIME = 0.1f; // avoids a huge step after restoring a minimized window

void drawFrame(GpuContext& gpu) {
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
}

void Engine::run(IGame& game) {
    if (!glfwInit()) {
        throw std::runtime_error("Erro: falha ao inicializar GLFW.");
    }

    if (!glfwVulkanSupported()) {
        glfwTerminate();
        throw std::runtime_error("Erro: Vulkan nao esta disponivel no sistema via GLFW.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Mush2D Engine - Vulkan", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Erro: falha ao criar a janela.");
    }

    GpuContext gpu;
    GlfwInput input;
    Camera2D camera{};
    Scene2D scene{};
    EngineServices services{gpu, input, camera, scene};

    try {
        input.attachWindow(window);
        gpu.init(window);
        game.init(services);

        float lastTime = static_cast<float>(glfwGetTime());
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            input.beginFrame();
            input.poll();

            float currentTime = static_cast<float>(glfwGetTime());
            float deltaTime = std::min(currentTime - lastTime, MAX_DELTA_TIME);
            lastTime = currentTime;

            game.update(services, deltaTime);

            camera.updateFollow(scene, deltaTime);
            camera.clampToBounds(gpu.swapchain().extent());

            drawFrame(gpu);
        }

        game.shutdown(services);
    } catch (...) {
        scene.clear();
        gpu.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        throw;
    }

    scene.clear();
    gpu.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
}
