#pragma once

#include "engine/gpu/Swapchain.hpp"

#include <array>
#include <functional>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

struct GLFWwindow;

struct FrameContext {
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore acquire = VK_NULL_HANDLE;    // per frame in flight
    VkSemaphore renderDone = VK_NULL_HANDLE; // per swapchain image
    uint32_t imageIndex = 0;
};

class GpuContext {
public:
    static constexpr uint32_t FramesInFlight = 2;

    void init(GLFWwindow* window);
    void shutdown();

    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physicalDevice() const { return physical_; }
    VkDevice device() const { return device_; }
    VmaAllocator allocator() const { return allocator_; }
    VkQueue graphicsQueue() const { return graphicsQueue_; }
    uint32_t graphicsFamily() const { return graphicsFamily_; }
    Swapchain& swapchain() { return swapchain_; }
    uint32_t frameIndex() const { return frameIndex_; }

    // nullptr = skip this frame (swapchain recreated / minimized). The caller records into
    // frame.cmd and must leave the swapchain image in PRESENT_SRC_KHR before endFrame().
    FrameContext* beginFrame();
    void endFrame();

    void immediateSubmit(std::function<void(VkCommandBuffer)> fn);

    // Called after the swapchain is recreated, with the new extent.
    std::function<void(VkExtent2D)> onResize;

private:
    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE;
        FrameContext ctx;
    };

    void resize();

    GLFWwindow* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    uint32_t graphicsFamily_ = 0;

    Swapchain swapchain_;
    std::array<Frame, FramesInFlight> frames_;
    uint32_t frameIndex_ = 0;

    VkCommandPool immPool_ = VK_NULL_HANDLE;
    VkCommandBuffer immCmd_ = VK_NULL_HANDLE;
    VkFence immFence_ = VK_NULL_HANDLE;
};
