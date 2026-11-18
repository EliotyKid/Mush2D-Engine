#include "engine/render/GpuTimers.hpp"

#include <iostream>

void GpuTimers::init(GpuContext& gpu) {
    gpu_ = &gpu;
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(gpu.physicalDevice(), &props);
    uint32_t n = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu.physicalDevice(), &n, nullptr);
    std::vector<VkQueueFamilyProperties> families(n);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu.physicalDevice(), &n, families.data());
    uint32_t bits = gpu.graphicsFamily() < n ? families[gpu.graphicsFamily()].timestampValidBits : 0;
    if (bits == 0) {
        std::cout << "GpuTimers: fila grafica sem suporte a timestamps; timers vazios.\n";
        return;
    }
    mask_ = bits >= 64 ? ~0ull : (1ull << bits) - 1;
    periodNs_ = props.limits.timestampPeriod;

    VkQueryPoolCreateInfo ci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    ci.queryCount = GpuContext::FramesInFlight * MaxPasses * 2;
    if (vkCreateQueryPool(gpu.device(), &ci, nullptr, &pool_) != VK_SUCCESS) mask_ = 0;
}

void GpuTimers::shutdown() {
    if (pool_ != VK_NULL_HANDLE) vkDestroyQueryPool(gpu_->device(), pool_, nullptr);
    *this = {};
}

void GpuTimers::beginFrame(VkCommandBuffer cmd) {
    if (!mask_) return;
    uint32_t slotIndex = gpu_->frameIndex();
    Slot& slot = slots_[slotIndex];
    uint32_t first = slotIndex * MaxPasses * 2;

    if (!slot.names.empty()) {
        uint32_t count = static_cast<uint32_t>(slot.names.size()) * 2;
        std::vector<uint64_t> data(count * 2); // value, availability
        VkResult r = vkGetQueryPoolResults(gpu_->device(), pool_, first, count, data.size() * sizeof(uint64_t),
                                           data.data(), 2 * sizeof(uint64_t),
                                           VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        bool ready = r == VK_SUCCESS;
        for (uint32_t i = 0; i < count && ready; ++i) ready = data[i * 2 + 1] != 0;
        if (ready) {
            last_.clear();
            for (size_t p = 0; p < slot.names.size(); ++p) {
                uint64_t ticks = (data[(p * 2 + 1) * 2] - data[p * 2 * 2]) & mask_;
                last_.emplace_back(slot.names[p], static_cast<float>(ticks * periodNs_ * 1e-6));
            }
        }
    }
    slot.names.clear();
    vkCmdResetQueryPool(cmd, pool_, first, MaxPasses * 2);
}

void GpuTimers::begin(VkCommandBuffer cmd, const char* pass) {
    if (!mask_) return;
    uint32_t slotIndex = gpu_->frameIndex();
    Slot& slot = slots_[slotIndex];
    if (slot.names.size() >= MaxPasses) return;
    slot.names.emplace_back(pass);
    vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, pool_,
                         slotIndex * MaxPasses * 2 + static_cast<uint32_t>(slot.names.size() - 1) * 2);
}

void GpuTimers::end(VkCommandBuffer cmd) {
    if (!mask_) return;
    uint32_t slotIndex = gpu_->frameIndex();
    const Slot& slot = slots_[slotIndex];
    if (slot.names.empty()) return;
    vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, pool_,
                         slotIndex * MaxPasses * 2 + static_cast<uint32_t>(slot.names.size() - 1) * 2 + 1);
}