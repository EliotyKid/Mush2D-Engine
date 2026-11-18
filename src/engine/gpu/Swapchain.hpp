#pragma once

#include <vector>

#include <vulkan/vulkan.h>

struct GLFWwindow;

// Owns the VkSwapchainKHR, its image views and one renderDone semaphore PER IMAGE
// (a semaphore handed to present can only be reused once that image is re-acquired).
class Swapchain {
public:
    void init(VkPhysicalDevice physical, VkDevice device, VkSurfaceKHR surface,
              uint32_t graphicsFamily, uint32_t presentFamily, GLFWwindow* window);
    // False (and nothing changed) while the framebuffer is 0x0.
    bool recreate();
    void destroy();

    VkSwapchainKHR handle() const { return swapchain_; }
    VkFormat format() const { return format_; }
    VkExtent2D extent() const { return extent_; }
    uint32_t imageCount() const { return static_cast<uint32_t>(images_.size()); }
    VkImage image(uint32_t i) const { return images_[i]; }
    VkImageView view(uint32_t i) const { return views_[i]; }
    VkSemaphore renderDone(uint32_t i) const { return renderDone_[i]; }

private:
    void destroyViewsAndSemaphores();

    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    uint32_t graphicsFamily_ = 0;
    uint32_t presentFamily_ = 0;
    GLFWwindow* window_ = nullptr;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<VkImageView> views_;
    std::vector<VkSemaphore> renderDone_;
};
