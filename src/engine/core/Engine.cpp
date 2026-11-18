#include "engine/core/Engine.hpp"
#include "engine/gpu/Buffer.hpp"
#include "engine/gpu/Image.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;
constexpr float MAX_DELTA_TIME = 0.1f; // avoids a huge step after restoring a minimized window
constexpr uint32_t SCREENSHOT_FRAME = 60;

// Dev tool (MUSH2D_SCREENSHOT=<file.png>): copies the presented image into a host buffer.
// Call after the renderer, before endFrame; the image is back in PRESENT_SRC_KHR afterwards.
GpuBuffer recordScreenshot(GpuContext& gpu, FrameContext& frame) {
    VkExtent2D e = gpu.swapchain().extent();
    GpuBuffer buf = createHostBuffer(gpu, VkDeviceSize{e.width} * e.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
    VkImage image = gpu.swapchain().image(frame.imageIndex);
    transition(frame.cmd, image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {e.width, e.height, 1};
    vkCmdCopyImageToBuffer(frame.cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf.buffer, 1, &region);
    transition(frame.cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkMemoryBarrier2 toHost{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    toHost.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    toHost.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toHost.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    toHost.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
    VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.memoryBarrierCount = 1;
    di.pMemoryBarriers = &toHost;
    vkCmdPipelineBarrier2(frame.cmd, &di);
    return buf;
}

// Waits for the GPU, converts BGRA -> RGBA and writes the PNG.
void saveScreenshot(GpuContext& gpu, GpuBuffer& buf, VkExtent2D e, const std::string& path) {
    vkDeviceWaitIdle(gpu.device());
    vmaInvalidateAllocation(gpu.allocator(), buf.alloc, 0, VK_WHOLE_SIZE);
    auto* px = static_cast<uint8_t*>(buf.mapped);
    for (VkDeviceSize i = 0; i < buf.size; i += 4) {
        std::swap(px[i], px[i + 2]);
        px[i + 3] = 255;
    }
    int w = static_cast<int>(e.width), h = static_cast<int>(e.height);
    if (!stbi_write_png(path.c_str(), w, h, 4, px, w * 4)) {
        std::cerr << "Screenshot: falha ao gravar " << path << '\n';
    } else {
        std::cerr << "Screenshot: " << path << '\n';
    }
    destroyBuffer(gpu, buf);
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
    Bindless bindless;
    MaterialLibrary materials;
    SceneRenderer renderer;
    EngineServices services{gpu, renderer, materials, input, camera, scene};
    VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    bool imguiReady = false;
    auto teardown = [&] {
        if (imguiReady) {
            vkDeviceWaitIdle(gpu.device());
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            vkDestroyDescriptorPool(gpu.device(), imguiPool, nullptr);
        }
        scene.clear();
        renderer.shutdown();
        materials.shutdown();
        bindless.shutdown();
        gpu.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    const char* screenshotEnv = std::getenv("MUSH2D_SCREENSHOT");
    std::string screenshotPath = screenshotEnv ? screenshotEnv : "";
    uint32_t framesRendered = 0;

    // Dev tool (MUSH2D_PERF_LOG=<seconds>): logs fps and average per-pass GPU ms each second, exits after N s.
    const char* perfEnv = std::getenv("MUSH2D_PERF_LOG");
    const float perfSeconds = perfEnv ? static_cast<float>(std::atof(perfEnv)) : 0.0f;
    float perfStart = -1.0f, perfTick = 0.0f;
    uint32_t perfFrames = 0;
    std::map<std::string, float> perfSum;

    try {
        input.attachWindow(window);
        gpu.init(window);
        bindless.init(gpu);
        materials.init(gpu, bindless);
        renderer.init(gpu, bindless);
        gpu.onResize = [&](VkExtent2D extent) { renderer.resize(extent); };
        game.init(services);

        // Dear ImGui (after attachWindow: it chains our scroll callback). Own descriptor pool, dynamic rendering.
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 16;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(gpu.device(), &poolInfo, nullptr, &imguiPool) != VK_SUCCESS) {
            throw std::runtime_error("Erro: falha ao criar o descriptor pool do ImGui.");
        }
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr; // no imgui.ini next to the exe
        ImGui_ImplGlfw_InitForVulkan(window, true);
        VkFormat swapFormat = gpu.swapchain().format();
        ImGui_ImplVulkan_InitInfo vkInit{};
        vkInit.Instance = gpu.instance();
        vkInit.PhysicalDevice = gpu.physicalDevice();
        vkInit.Device = gpu.device();
        vkInit.QueueFamily = gpu.graphicsFamily();
        vkInit.Queue = gpu.graphicsQueue();
        vkInit.DescriptorPool = imguiPool;
        vkInit.MinImageCount = 2;
        vkInit.ImageCount = std::max(2u, gpu.swapchain().imageCount());
        vkInit.UseDynamicRendering = true;
        vkInit.PipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        vkInit.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        vkInit.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapFormat;
        ImGui_ImplVulkan_Init(&vkInit);
        imguiReady = true;

        bool showPanel = std::getenv("MUSH2D_SHOW_PANEL") != nullptr; // F1 toggles
        bool f1Held = false;
        ImGuiIO& io = ImGui::GetIO();

        float lastTime = static_cast<float>(glfwGetTime());
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            bool f1 = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
            if (f1 && !f1Held) showPanel = !showPanel;
            f1Held = f1;
            // Hidden panel never blocks the game (io.Want* is stale from the last drawn frame).
            input.setBlocked(showPanel && io.WantCaptureKeyboard, showPanel && io.WantCaptureMouse);
            if (showPanel) {
                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
            }
            input.beginFrame();
            input.poll();

            float currentTime = static_cast<float>(glfwGetTime());
            float deltaTime = std::min(currentTime - lastTime, MAX_DELTA_TIME);
            lastTime = currentTime;

            game.update(services, deltaTime);

            camera.pixelArt = renderer.settings().pixelArt;
            camera.updateFollow(scene, deltaTime);
            camera.clampToBounds(gpu.swapchain().extent());

            if (showPanel) {
                game.onImGui(services);
                ImGui::Render();
            }

            FrameContext* frame = gpu.beginFrame();
            if (!frame) {
                continue;
            }
            renderer.overlay = showPanel ? std::function<void(VkCommandBuffer)>{[](VkCommandBuffer cmd) {
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
            }} : nullptr;
            renderer.render(*frame, camera, currentTime);

            if (perfSeconds > 0.0f) {
                if (perfStart < 0.0f) perfStart = perfTick = currentTime;
                ++perfFrames;
                for (const auto& [name, ms] : renderer.timers().lastFrameMs()) perfSum[name] += ms;
                if (currentTime - perfTick >= 1.0f) {
                    float dt = currentTime - perfTick, total = 0.0f;
                    std::cout << "perf fps=" << perfFrames / dt << " frame_ms=" << 1000.0f * dt / perfFrames;
                    for (const auto& [name, sum] : perfSum) {
                        std::cout << " " << name << "=" << sum / perfFrames;
                        total += sum / perfFrames;
                    }
                    std::cout << " sum=" << total << std::endl;
                    perfTick = currentTime;
                    perfFrames = 0;
                    perfSum.clear();
                }
                if (currentTime - perfStart >= perfSeconds) glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            bool shoot = !screenshotPath.empty() && ++framesRendered == SCREENSHOT_FRAME;
            GpuBuffer shot;
            VkExtent2D shotExtent = gpu.swapchain().extent();
            if (shoot) shot = recordScreenshot(gpu, *frame);
            gpu.endFrame();
            if (shoot) {
                saveScreenshot(gpu, shot, shotExtent, screenshotPath);
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }

        game.shutdown(services);
    } catch (...) {
        teardown();
        throw;
    }

    teardown();
}
