#pragma once

#include "engine/gpu/Image.hpp"
#include "engine/render/Bindless.hpp"

#include <array>

// G-buffer (native resolution) + HDR light target (native or reduced) + native additive target. Each image is sampled through
// Bindless; the indices stay fixed across resizes (re-registered with Bindless::set).
class RenderTargets {
public:
    static constexpr int BloomLevels = 5; // 1/2 .. 1/32 of the swapchain extent
    enum Target { Albedo, Normal, Emissive, Depth, Light, Additive, Count };
    static constexpr std::array<VkFormat, Count> Formats = {
        VK_FORMAT_R8G8B8A8_SRGB,       // albedo
        VK_FORMAT_R8G8B8A8_UNORM,      // normal: RG normal, B rim, A AO
        VK_FORMAT_R16G16B16A16_SFLOAT, // emissive
        VK_FORMAT_R16_SFLOAT,          // layer depth
        VK_FORMAT_R16G16B16A16_SFLOAT, // HDR light
        VK_FORMAT_R16G16B16A16_SFLOAT, // additive sprites, always native resolution
    };

    // Destroys any previous images first; caller guarantees the GPU no longer uses them.
    void create(GpuContext& gpu, Bindless& bindless, VkExtent2D extent, VkExtent2D lightExtent);
    void destroy(GpuContext& gpu);

    const GpuImage& image(Target t) const { return images_[t]; }
    uint32_t index(Target t) const { return indices_[t]; }
    VkExtent2D extent() const { return images_[Albedo].extent; }
    VkExtent2D lightExtent() const { return images_[Light].extent; }
    const GpuImage& bloom(int level) const { return bloom_[level]; }
    uint32_t bloomIndex(int level) const { return bloomIndices_[level]; }

private:
    std::array<GpuImage, Count> images_{};
    std::array<uint32_t, Count> indices_{};
    std::array<GpuImage, BloomLevels> bloom_{};
    std::array<uint32_t, BloomLevels> bloomIndices_{};
    bool registered_ = false;
};
