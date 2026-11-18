#include "engine/gpu/Swapchain.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <stdexcept>
#include <string>

void Swapchain::init(VkPhysicalDevice physical, VkDevice device, VkSurfaceKHR surface,
                     uint32_t graphicsFamily, uint32_t presentFamily, GLFWwindow* window) {
    physical_ = physical;
    device_ = device;
    surface_ = surface;
    graphicsFamily_ = graphicsFamily;
    presentFamily_ = presentFamily;
    window_ = window;
    if (!recreate()) {
        throw std::runtime_error("Swapchain: janela com framebuffer 0x0 na inicializacao.");
    }
}

bool Swapchain::recreate() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    if (w == 0 || h == 0) return false;

    vkDeviceWaitIdle(device_);

    vkb::SwapchainBuilder builder{physical_, device_, surface_, graphicsFamily_, presentFamily_};
    auto result = builder
        .set_desired_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(static_cast<uint32_t>(w), static_cast<uint32_t>(h))
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .set_old_swapchain(swapchain_)
        .build();
    if (!result) {
        throw std::runtime_error("Swapchain: " + result.error().message());
    }
    vkb::Swapchain built = result.value();
    if (built.image_format != VK_FORMAT_B8G8R8A8_UNORM) {
        throw std::runtime_error("Swapchain: B8G8R8A8_UNORM indisponivel.");
    }

    destroyViewsAndSemaphores();
    if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);

    swapchain_ = built.swapchain;
    format_ = built.image_format;
    extent_ = built.extent;
    images_ = built.get_images().value();
    views_ = built.get_image_views().value();

    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    renderDone_.resize(images_.size());
    for (auto& s : renderDone_) {
        if (vkCreateSemaphore(device_, &sci, nullptr, &s) != VK_SUCCESS) {
            throw std::runtime_error("Swapchain: falha ao criar semaforo.");
        }
    }
    return true;
}

void Swapchain::destroyViewsAndSemaphores() {
    for (auto v : views_) vkDestroyImageView(device_, v, nullptr);
    for (auto s : renderDone_) vkDestroySemaphore(device_, s, nullptr);
    views_.clear();
    renderDone_.clear();
    images_.clear();
}

void Swapchain::destroy() {
    if (device_ == VK_NULL_HANDLE) return;
    destroyViewsAndSemaphores();
    if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}
