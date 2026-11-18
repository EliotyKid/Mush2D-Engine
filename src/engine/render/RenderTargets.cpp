#include "engine/render/RenderTargets.hpp"

#include <algorithm>

void RenderTargets::create(GpuContext& gpu, Bindless& bindless, VkExtent2D extent, VkExtent2D lightExtent) {
    destroy(gpu);
    for (int t = 0; t < Count; ++t) {
        images_[t] = createImage(gpu, t == Light ? lightExtent : extent, Formats[t],
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        if (registered_) {
            bindless.set(indices_[t], images_[t].view);
        } else {
            indices_[t] = bindless.add(images_[t].view);
        }
    }
    for (int i = 0; i < BloomLevels; ++i) {
        VkExtent2D e{std::max(1u, extent.width >> (i + 1)), std::max(1u, extent.height >> (i + 1))};
        bloom_[i] = createImage(gpu, e, Formats[Light],
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        if (registered_) {
            bindless.set(bloomIndices_[i], bloom_[i].view);
        } else {
            bloomIndices_[i] = bindless.add(bloom_[i].view);
        }
    }
    registered_ = true;
}

void RenderTargets::destroy(GpuContext& gpu) {
    for (GpuImage& img : images_) destroyImage(gpu, img);
    for (GpuImage& img : bloom_) destroyImage(gpu, img);
}
