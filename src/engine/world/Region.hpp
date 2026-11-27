#pragma once

#include "engine/render/Materials.hpp"
#include "engine/tools/LookFile.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

class Camera2D;
class MaterialLibrary;
class SpriteBatch;

// One background image. offset = world position of copy 0's top-left corner (Y up), anchored at the world
// origin: with the camera at 0 it shows exactly there. repeatX tiles copies every image width.
struct RegionLayer {
    std::string image; // relative to the executable dir (assetPath), e.g. "assets/art/parallax/bg1.png"
    float depth;
    glm::vec2 parallax;
    glm::vec2 offset;
    bool repeatX;
    std::optional<Material> material; // set by resolveRegionMaterials; empty = unreadable image, not drawn
};

// assets/regions/<name>.region.json: { "look": <.look.json format>, "layers": [RegionLayer...], "music": "<name>" }.
struct Region {
    std::string name; // file name up to the first '.'; not stored in the file
    std::string music; // assets/audio/music/<music>.wav, looped while the region is current; "" = silence
    LookFile look;
    std::vector<RegionLayer> layers;
};

// nullopt if the text is not a JSON object; a missing "look" keeps `defaults`, malformed layers are skipped.
std::optional<Region> parseRegion(const std::string& json, const FrameSettings& defaults);
std::string serializeRegion(const Region&);
// Missing or corrupt file: warns on stderr and returns a region with `defaults` and no layers.
Region loadRegionOrDefault(const std::filesystem::path&, const FrameSettings& defaults);
void saveRegion(const std::filesystem::path&, const Region&);

// Float and vec3 fields interpolate; int and bool fields take b's value once t >= 0.5.
FrameSettings lerpSettings(const FrameSettings& a, const FrameSettings& b, float t);

// Copies k (inclusive range x..y) of a layer whose image is imageWidth wide that touch the view
// [camera.x * parallax.x - halfWidth, camera.x * parallax.x + halfWidth]; always (0, 0) without repeatX.
glm::ivec2 regionCopyRange(const RegionLayer&, float imageWidth, glm::vec2 cameraPos, float halfWidth);
// Once, when the region loads: loads every layer image; an unreadable one warns and leaves material empty.
void resolveRegionMaterials(Region&, MaterialLibrary&);
// Emits the visible copies of every layer with a resolved material.
void appendRegionLayers(const Region&, const Camera2D&, VkExtent2D viewport, SpriteBatch&);
