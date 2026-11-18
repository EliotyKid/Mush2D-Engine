#include "engine/render/Materials.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>

MaterialPaths resolveMaterialPaths(const std::filesystem::path& albedo) {
    MaterialPaths r;
    r.albedo = albedo;
    auto find = [&](const char* suffix) -> std::optional<std::filesystem::path> {
        auto p = albedo.parent_path() / (albedo.stem().string() + suffix + ".png");
        if (std::filesystem::exists(p)) return p;
        return std::nullopt;
    };
    r.normal = find("_n");
    r.emissive = find("_e");
    r.mask = find("_m");
    return r;
}

bool mapMatchesAlbedo(int aw, int ah, int mw, int mh) { return aw == mw && ah == mh; }

void MaterialLibrary::init(GpuContext& gpu, Bindless& bindless) {
    gpu_ = &gpu;
    bindless_ = &bindless;
    const uint8_t normal[4] = {128, 128, 255, 255};
    const uint8_t emissive[4] = {0, 0, 0, 255};
    const uint8_t mask[4] = {0, 255, 0, 255}; // R = rim, G = AO
    neutralNormal_ = upload(normal, 1, 1, VK_FORMAT_R8G8B8A8_UNORM);
    neutralEmissive_ = upload(emissive, 1, 1, VK_FORMAT_R8G8B8A8_UNORM);
    neutralMask_ = upload(mask, 1, 1, VK_FORMAT_R8G8B8A8_UNORM);
}

void MaterialLibrary::shutdown() {
    if (!gpu_) return;
    vkDeviceWaitIdle(gpu_->device());
    for (GpuImage& img : images_) destroyImage(*gpu_, img);
    images_.clear();
    cache_.clear();
    gpu_ = nullptr;
}

uint32_t MaterialLibrary::upload(const uint8_t* rgba, uint32_t w, uint32_t h, VkFormat format) {
    const VkDeviceSize size = VkDeviceSize(w) * h * 4;

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation stagingAlloc = VK_NULL_HANDLE;
    VmaAllocationInfo info{};
    if (vmaCreateBuffer(gpu_->allocator(), &bci, &aci, &staging, &stagingAlloc, &info) != VK_SUCCESS) {
        throw std::runtime_error("Materials: staging buffer falhou.");
    }
    std::memcpy(info.pMappedData, rgba, size);
    vmaFlushAllocation(gpu_->allocator(), stagingAlloc, 0, VK_WHOLE_SIZE);

    GpuImage img;
    try {
        img = createImage(*gpu_, {w, h}, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        gpu_->immediateSubmit([&](VkCommandBuffer cmd) {
            transition(cmd, img.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            VkBufferImageCopy c{};
            c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            c.imageExtent = {w, h, 1};
            vkCmdCopyBufferToImage(cmd, staging, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
            transition(cmd, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
    } catch (...) {
        vmaDestroyBuffer(gpu_->allocator(), staging, stagingAlloc);
        if (img.image) destroyImage(*gpu_, img);
        throw;
    }
    vmaDestroyBuffer(gpu_->allocator(), staging, stagingAlloc);

    images_.push_back(img);
    return bindless_->add(img.view);
}

uint32_t MaterialLibrary::loadTexture(const std::filesystem::path& path, bool srgb) {
    int w = 0, h = 0, n = 0;
    stbi_uc* px = stbi_load(path.string().c_str(), &w, &h, &n, 4);
    if (!px) throw std::runtime_error("Materials: falha ao carregar " + path.string());
    uint32_t idx;
    try {
        idx = upload(px, uint32_t(w), uint32_t(h), srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM);
    } catch (...) {
        stbi_image_free(px);
        throw;
    }
    stbi_image_free(px);
    return idx;
}

uint32_t MaterialLibrary::loadMap(const std::optional<std::filesystem::path>& p, int aw, int ah, uint32_t neutral) {
    if (!p) return neutral;
    int mw = 0, mh = 0, n = 0;
    if (!stbi_info(p->string().c_str(), &mw, &mh, &n)) {
        std::fprintf(stderr, "[materials] aviso: mapa ilegivel %s, usando neutro\n", p->string().c_str());
        return neutral;
    }
    if (!mapMatchesAlbedo(aw, ah, mw, mh)) {
        std::fprintf(stderr, "[materials] aviso: %s e %dx%d, albedo e %dx%d; usando neutro\n",
                     p->string().c_str(), mw, mh, aw, ah);
        return neutral;
    }
    return loadTexture(*p, false);
}

Material MaterialLibrary::load(const std::filesystem::path& albedoPng) {
    const std::string key = std::filesystem::absolute(albedoPng).lexically_normal().string();
    if (auto it = cache_.find(key); it != cache_.end()) return it->second;
    MaterialPaths paths = resolveMaterialPaths(albedoPng);
    int aw = 0, ah = 0, n = 0;
    if (!stbi_info(albedoPng.string().c_str(), &aw, &ah, &n)) {
        throw std::runtime_error("Materials: albedo ilegivel " + albedoPng.string());
    }
    Material m;
    m.albedo = loadTexture(albedoPng, true);
    m.normal = loadMap(paths.normal, aw, ah, neutralNormal_);
    m.emissive = loadMap(paths.emissive, aw, ah, neutralEmissive_);
    m.mask = loadMap(paths.mask, aw, ah, neutralMask_);
    cache_[key] = m;
    return m;
}
