#pragma once

#include "engine/gpu/GpuContext.hpp"

// Global bindless set: binding 0 = texture2D[Capacity] (partially bound, update-after-bind),
// binding 1 = sampler[2] (0 = NEAREST, 1 = LINEAR, clamp-to-edge; immutable).
class Bindless {
public:
    static constexpr uint32_t Capacity = 1024;

    void init(GpuContext& gpu);
    void shutdown();

    uint32_t add(VkImageView view); // returns the new index; throws when full
    void set(uint32_t index, VkImageView view);

    VkDescriptorSetLayout layout() const { return layout_; }
    VkDescriptorSet descriptorSet() const { return set_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    VkSampler samplers_[2]{};
    uint32_t count_ = 0;
};
