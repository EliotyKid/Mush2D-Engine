#include "engine/world/Region.hpp"

#include "engine/core/Paths.hpp"
#include "engine/graphics/Camera2D.hpp"
#include "engine/render/Materials.hpp"
#include "engine/render/SpriteBatch.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {
// 6 significant digits, as LookFile: readable files (0.2f -> 0.2).
double r6(float v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.6g", double(v));
    return std::strtod(b, nullptr);
}
bool vec2(const json& o, const char* key, glm::vec2& out) {
    auto it = o.find(key);
    if (it == o.end() || !it->is_array() || it->size() != 2 || !(*it)[0].is_number() || !(*it)[1].is_number()) return false;
    out = {(*it)[0].get<float>(), (*it)[1].get<float>()};
    return true;
}
}

std::optional<Region> parseRegion(const std::string& text, const FrameSettings& defaults) {
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;

    Region r;
    r.look.settings = defaults;
    if (auto it = j.find("look"); it != j.end() && it->is_object()) {
        if (auto look = parseLook(it->dump(), defaults)) r.look = *look;
    }
    if (auto it = j.find("layers"); it != j.end() && it->is_array()) {
        for (const json& l : *it) {
            RegionLayer layer{"", 0.0f, {1.0f, 1.0f}, {0.0f, 0.0f}, false};
            if (!l.is_object() || !l.contains("image") || !l["image"].is_string()) {
                std::fprintf(stderr, "Region: layer without image, skipped\n");
                continue;
            }
            layer.image = l["image"].get<std::string>();
            if (l.contains("depth") && l["depth"].is_number()) layer.depth = l["depth"].get<float>();
            vec2(l, "parallax", layer.parallax);
            vec2(l, "offset", layer.offset);
            if (l.contains("repeatX") && l["repeatX"].is_boolean()) layer.repeatX = l["repeatX"].get<bool>();
            r.layers.push_back(std::move(layer));
        }
    }
    return r;
}

std::string serializeRegion(const Region& r) {
    json j;
    j["look"] = json::parse(serializeLook(r.look)); // same rounding and layout as .look.json
    j["layers"] = json::array();
    for (const RegionLayer& l : r.layers) {
        j["layers"].push_back({{"image", l.image}, {"depth", r6(l.depth)}, {"parallax", {r6(l.parallax.x), r6(l.parallax.y)}},
                               {"offset", {r6(l.offset.x), r6(l.offset.y)}}, {"repeatX", l.repeatX}});
    }
    return j.dump(2) + "\n";
}

Region loadRegionOrDefault(const std::filesystem::path& path, const FrameSettings& defaults) {
    Region r;
    r.look.settings = defaults;
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "Region: %s not found, using defaults\n", path.string().c_str());
    } else {
        std::stringstream ss;
        ss << in.rdbuf();
        if (auto parsed = parseRegion(ss.str(), defaults)) r = std::move(*parsed);
        else std::fprintf(stderr, "Region: %s is corrupt, using defaults\n", path.string().c_str());
    }
    const std::string file = path.filename().string();
    r.name = file.substr(0, file.find('.'));
    return r;
}

void saveRegion(const std::filesystem::path& path, const Region& r) {
    std::ofstream out(path, std::ios::trunc);
    out << serializeRegion(r);
    if (!out) std::fprintf(stderr, "Region: failed to write %s\n", path.string().c_str());
}

FrameSettings lerpSettings(const FrameSettings& a, const FrameSettings& b, float t) {
    // New FrameSettings fields must be added here, or they jump to b at once.
    FrameSettings s = t >= 0.5f ? b : a; // ints and bools
    auto f = [t](float x, float y) { return x + (y - x) * t; };
    auto v = [t](glm::vec3 x, glm::vec3 y) { return x + (y - x) * t; };
    s.ambientColor = v(a.ambientColor, b.ambientColor); s.ambientIntensity = f(a.ambientIntensity, b.ambientIntensity);
    s.fogColor = v(a.fogColor, b.fogColor); s.fogDensity = f(a.fogDensity, b.fogDensity);
    s.fogGradientStart = f(a.fogGradientStart, b.fogGradientStart);
    s.fogGradientEnd = f(a.fogGradientEnd, b.fogGradientEnd);
    s.fogGradientStrength = f(a.fogGradientStrength, b.fogGradientStrength);
    s.exposure = f(a.exposure, b.exposure);
    s.bloomThreshold = f(a.bloomThreshold, b.bloomThreshold); s.bloomKnee = f(a.bloomKnee, b.bloomKnee);
    s.bloomIntensity = f(a.bloomIntensity, b.bloomIntensity);
    s.lift = v(a.lift, b.lift); s.gamma = v(a.gamma, b.gamma); s.gain = v(a.gain, b.gain);
    s.saturation = f(a.saturation, b.saturation); s.temperature = f(a.temperature, b.temperature);
    s.fade = f(a.fade, b.fade);
    return s;
}

glm::ivec2 regionCopyRange(const RegionLayer& l, float imageWidth, glm::vec2 cameraPos, float halfWidth) {
    if (!l.repeatX || imageWidth <= 0.0f) return {0, 0};
    const float centre = cameraPos.x * l.parallax.x - l.offset.x; // view centre relative to copy 0's left edge
    return {int(std::floor((centre - halfWidth) / imageWidth)), int(std::floor((centre + halfWidth) / imageWidth))};
}

void resolveRegionMaterials(Region& region, MaterialLibrary& materials) {
    for (RegionLayer& l : region.layers) {
        try {
            l.material = materials.load(assetPath(l.image));
        } catch (const std::exception& e) {
            l.material.reset();
            std::fprintf(stderr, "Region %s: layer skipped: %s\n", region.name.c_str(), e.what());
        }
    }
}

void appendRegionLayers(const Region& region, const Camera2D& camera, VkExtent2D viewport, SpriteBatch& batch) {
    constexpr float PixelsPerUnit = 32.0f;
    // The game submits before the camera follows this frame: pad so a moving camera never shows a missing copy.
    const float halfWidth = camera.halfExtent(viewport).x + 2.0f;
    for (const RegionLayer& l : region.layers) {
        if (!l.material) continue;
        const Material& m = *l.material;
        const glm::vec2 size = glm::vec2(m.width, m.height) / PixelsPerUnit;
        const glm::ivec2 range = regionCopyRange(l, size.x, camera.position, halfWidth);
        for (int k = range.x; k <= range.y; ++k) {
            SpriteInstance s{};
            s.position = l.offset + glm::vec2(k * size.x + 0.5f * size.x, -0.5f * size.y);
            s.size = size;
            s.uvRect = {0.0f, 0.0f, 1.0f, 1.0f};
            s.color = {1, 1, 1, 1};
            s.depth = l.depth;
            s.parallax = l.parallax;
            s.albedo = m.albedo; s.normal = m.normal; s.emissive = m.emissive; s.mask = m.mask;
            s.flags = SpriteFlagNearest;
            batch.add(s);
        }
    }
}
