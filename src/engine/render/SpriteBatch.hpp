#pragma once

#include "engine/render/Materials.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

class Scene2D;

// std430, 96 bytes. Same order in shaders/common.glsl.
struct SpriteInstance {
    glm::vec2 position; glm::vec2 size;      // size.x < 0 = mirrored
    glm::vec4 uvRect;   glm::vec4 color;
    float rotation; float depth; glm::vec2 parallax;  // parallax 1 = gameplay layer
    uint32_t albedo, normal, emissive, mask;
    uint32_t flags; uint32_t flash; uint32_t _pad[2]; // flags: bit0 NEAREST, bit1 ADDITIVE; flash: packUnorm4x8(rgb, amount)
};
static_assert(sizeof(SpriteInstance) == 96);

constexpr uint32_t SpriteFlagNearest = 1u << 0;
constexpr uint32_t SpriteFlagAdditive = 1u << 1;

class SpriteBatch {
public:
    void clear();
    void add(const SpriteInstance& instance, int order = 0);

    // Opaque sprites first, then additive ones; each range back to front: depth descending,
    // then order ascending; ties keep insertion order.
    const std::vector<SpriteInstance>& sorted();
    size_t opaqueCount() const { return opaqueCount_; } // valid after sorted()
    size_t size() const { return entries_.size(); }

private:
    struct Entry {
        SpriteInstance instance;
        int order;
    };
    std::vector<Entry> entries_;
    std::vector<SpriteInstance> sorted_;
    size_t opaqueCount_ = 0;
};

// Returns the atlas material and the frame's uvRect (x, y, w, h in 0..1). A uvRect with w <= 0 = frame missing.
using AtlasLookup = std::function<std::pair<Material, glm::vec4>(uint32_t atlasIndex, const std::string& frame)>;

// Every active, visible object with a sprite becomes one instance (size = world scale, times sprite.drawSize when set).
// Then the particles of every ParticleEmitter on an active object: a frame of the owner's sprite atlas, or `white`
// tinted when the particle has no frame (skipped when white is null).
void appendSceneSprites(const Scene2D& scene, SpriteBatch& batch, const AtlasLookup& lookup, const Material* white = nullptr);
