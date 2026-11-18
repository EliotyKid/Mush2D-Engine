#pragma once

#include "engine/gpu/Image.hpp"
#include "engine/render/Bindless.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

struct Material {
    uint32_t albedo = 0, normal = 0, emissive = 0, mask = 0; // bindless indices
};

struct MaterialPaths {
    std::filesystem::path albedo;
    std::optional<std::filesystem::path> normal, emissive, mask;
};

// Looks for <stem>_n.png, <stem>_e.png, <stem>_m.png next to the albedo.
MaterialPaths resolveMaterialPaths(const std::filesystem::path& albedo);
bool mapMatchesAlbedo(int aw, int ah, int mw, int mh);

class MaterialLibrary {
public:
    void init(GpuContext& gpu, Bindless& bindless); // creates the 3 neutral 1x1 textures
    void shutdown();                                // destroys every image (waits for device idle)

    // Albedo sRGB; missing or size-mismatched maps fall back to the neutral index.
    Material load(const std::filesystem::path& albedoPng);
    uint32_t loadTexture(const std::filesystem::path& path, bool srgb);

private:
    uint32_t upload(const uint8_t* rgba, uint32_t w, uint32_t h, VkFormat format);
    uint32_t loadMap(const std::optional<std::filesystem::path>& p, int aw, int ah, uint32_t neutral);

    GpuContext* gpu_ = nullptr;
    Bindless* bindless_ = nullptr;
    std::vector<GpuImage> images_;
    uint32_t neutralNormal_ = 0, neutralEmissive_ = 0, neutralMask_ = 0;
};
