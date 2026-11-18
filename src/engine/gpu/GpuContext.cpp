#include "engine/gpu/GpuContext.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <stdexcept>
#include <string>

namespace {
template <typename T>
T unwrap(vkb::Result<T> r, const char* what) {
    if (!r) throw std::runtime_error(std::string(what) + ": " + r.error().message());
    return r.value();
}
void vkCheck(VkResult r, const char* what) {
    if (r != VK_SUCCESS) throw std::runtime_error(std::string(what) + " falhou (VkResult " + std::to_string(r) + ")");
}
}

void GpuContext::init(GLFWwindow* window) {
    window_ = window;

    vkb::InstanceBuilder ib;
    ib.set_app_name("Mush2D").require_api_version(1, 3, 0);
#ifndef NDEBUG
    ib.request_validation_layers(true).use_default_debug_messenger();
#endif
    uint32_t extCount = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&extCount);
    if (!exts) throw std::runtime_error("GLFW nao reportou extensoes Vulkan.");
    for (uint32_t i = 0; i < extCount; ++i) ib.enable_extension(exts[i]);
    vkb::Instance vkbInstance = unwrap(ib.build(), "vkb instance");
    instance_ = vkbInstance.instance;
    debugMessenger_ = vkbInstance.debug_messenger;

    vkCheck(glfwCreateWindowSurface(instance_, window_, nullptr, &surface_), "glfwCreateWindowSurface");

    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    f13.dynamicRendering = VK_TRUE;
    f13.synchronization2 = VK_TRUE;
    f13.shaderDemoteToHelperInvocation = VK_TRUE; // glslc (vulkan1.3) lowers `discard` to demote
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.descriptorIndexing = VK_TRUE;
    f12.descriptorBindingPartiallyBound = VK_TRUE;
    f12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    f12.runtimeDescriptorArray = VK_TRUE;
    f12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{vkbInstance};
    vkb::PhysicalDevice vkbPhys = unwrap(
        selector.set_surface(surface_)
            .set_minimum_version(1, 3)
            .set_required_features_13(f13)
            .set_required_features_12(f12)
            .select(),
        "vkb physical device");
    physical_ = vkbPhys.physical_device;

    vkb::DeviceBuilder db{vkbPhys};
    vkb::Device vkbDevice = unwrap(db.build(), "vkb device");
    device_ = vkbDevice.device;
    graphicsQueue_ = unwrap(vkbDevice.get_queue(vkb::QueueType::graphics), "graphics queue");
    graphicsFamily_ = unwrap(vkbDevice.get_queue_index(vkb::QueueType::graphics), "graphics family");
    presentQueue_ = unwrap(vkbDevice.get_queue(vkb::QueueType::present), "present queue");
    uint32_t presentFamily = unwrap(vkbDevice.get_queue_index(vkb::QueueType::present), "present family");

    VmaAllocatorCreateInfo aci{};
    aci.physicalDevice = physical_;
    aci.device = device_;
    aci.instance = instance_;
    aci.vulkanApiVersion = VK_API_VERSION_1_3;
    vkCheck(vmaCreateAllocator(&aci, &allocator_), "vmaCreateAllocator");

    swapchain_.init(physical_, device_, surface_, graphicsFamily_, presentFamily, window_);

    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = graphicsFamily_;
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (auto& fr : frames_) {
        vkCheck(vkCreateCommandPool(device_, &pci, nullptr, &fr.pool), "frame pool");
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = fr.pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        vkCheck(vkAllocateCommandBuffers(device_, &ai, &fr.ctx.cmd), "frame cmd");
        vkCheck(vkCreateFence(device_, &fci, nullptr, &fr.ctx.fence), "frame fence");
        vkCheck(vkCreateSemaphore(device_, &sci, nullptr, &fr.ctx.acquire), "acquire semaphore");
    }

    vkCheck(vkCreateCommandPool(device_, &pci, nullptr, &immPool_), "imm pool");
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = immPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    vkCheck(vkAllocateCommandBuffers(device_, &ai, &immCmd_), "imm cmd");
    VkFenceCreateInfo ifci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCheck(vkCreateFence(device_, &ifci, nullptr, &immFence_), "imm fence");
}

void GpuContext::shutdown() {
    if (device_ == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device_);

    vkDestroyFence(device_, immFence_, nullptr);
    vkDestroyCommandPool(device_, immPool_, nullptr);
    for (auto& fr : frames_) {
        vkDestroySemaphore(device_, fr.ctx.acquire, nullptr);
        vkDestroyFence(device_, fr.ctx.fence, nullptr);
        vkDestroyCommandPool(device_, fr.pool, nullptr);
    }
    swapchain_.destroy();
    vmaDestroyAllocator(allocator_);
    vkDestroyDevice(device_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (debugMessenger_ != VK_NULL_HANDLE) vkb::destroy_debug_utils_messenger(instance_, debugMessenger_);
    vkDestroyInstance(instance_, nullptr);

    device_ = VK_NULL_HANDLE;
    instance_ = VK_NULL_HANDLE;
}

void GpuContext::resize() {
    if (swapchain_.recreate() && onResize) onResize(swapchain_.extent());
}

FrameContext* GpuContext::beginFrame() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    if (w == 0 || h == 0) {
        glfwWaitEvents(); // minimized: sleep until something happens
        return nullptr;
    }
    VkExtent2D cur = swapchain_.extent();
    if (static_cast<uint32_t>(w) != cur.width || static_cast<uint32_t>(h) != cur.height) {
        resize();
        return nullptr;
    }

    Frame& fr = frames_[frameIndex_];
    vkCheck(vkWaitForFences(device_, 1, &fr.ctx.fence, VK_TRUE, UINT64_MAX), "wait fence");

    VkResult r = vkAcquireNextImageKHR(device_, swapchain_.handle(), UINT64_MAX,
                                       fr.ctx.acquire, VK_NULL_HANDLE, &fr.ctx.imageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        resize();
        return nullptr;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) throw std::runtime_error("vkAcquireNextImageKHR falhou");
    // SUBOPTIMAL: image and semaphore are valid; the swapchain is rebuilt after present.

    vkCheck(vkResetFences(device_, 1, &fr.ctx.fence), "reset fence");
    vkCheck(vkResetCommandPool(device_, fr.pool, 0), "reset pool");
    fr.ctx.renderDone = swapchain_.renderDone(fr.ctx.imageIndex);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(fr.ctx.cmd, &bi), "begin cmd");
    return &fr.ctx;
}

void GpuContext::endFrame() {
    Frame& fr = frames_[frameIndex_];
    vkCheck(vkEndCommandBuffer(fr.ctx.cmd), "end cmd");

    VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    wait.semaphore = fr.ctx.acquire;
    wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signal.semaphore = fr.ctx.renderDone;
    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkCommandBufferSubmitInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cb.commandBuffer = fr.ctx.cmd;
    VkSubmitInfo2 si{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    si.waitSemaphoreInfoCount = 1;
    si.pWaitSemaphoreInfos = &wait;
    si.commandBufferInfoCount = 1;
    si.pCommandBufferInfos = &cb;
    si.signalSemaphoreInfoCount = 1;
    si.pSignalSemaphoreInfos = &signal;
    vkCheck(vkQueueSubmit2(graphicsQueue_, 1, &si, fr.ctx.fence), "vkQueueSubmit2");

    VkSwapchainKHR sc = swapchain_.handle();
    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &fr.ctx.renderDone;
    pi.swapchainCount = 1;
    pi.pSwapchains = &sc;
    pi.pImageIndices = &fr.ctx.imageIndex;
    VkResult r = vkQueuePresentKHR(presentQueue_, &pi);

    frameIndex_ = (frameIndex_ + 1) % FramesInFlight;

    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        resize();
    } else if (r != VK_SUCCESS) {
        throw std::runtime_error("vkQueuePresentKHR falhou");
    }
}

void GpuContext::immediateSubmit(std::function<void(VkCommandBuffer)> fn) {
    vkCheck(vkResetFences(device_, 1, &immFence_), "imm reset fence");
    vkCheck(vkResetCommandPool(device_, immPool_, 0), "imm reset pool");
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(immCmd_, &bi), "imm begin");
    fn(immCmd_);
    vkCheck(vkEndCommandBuffer(immCmd_), "imm end");

    VkCommandBufferSubmitInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cb.commandBuffer = immCmd_;
    VkSubmitInfo2 si{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    si.commandBufferInfoCount = 1;
    si.pCommandBufferInfos = &cb;
    vkCheck(vkQueueSubmit2(graphicsQueue_, 1, &si, immFence_), "imm submit");
    vkCheck(vkWaitForFences(device_, 1, &immFence_, VK_TRUE, UINT64_MAX), "imm wait");
}
