#include "engine/render/RenderTargets.hpp"

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
    registered_ = true;
}

void RenderTargets::destroy(GpuContext& gpu) {
    for (GpuImage& img : images_) destroyImage(gpu, img);
}
