#include "engine/render/SpriteBatch.hpp"

#include "engine/world/Scene2D.hpp"

#include <algorithm>

void SpriteBatch::clear() {
    entries_.clear();
}

void SpriteBatch::add(const SpriteInstance& instance, int order) {
    entries_.push_back({instance, order});
}

const std::vector<SpriteInstance>& SpriteBatch::sorted() {
    std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        if (a.instance.depth != b.instance.depth) return a.instance.depth > b.instance.depth;
        return a.order < b.order;
    });
    sorted_.clear();
    for (const Entry& e : entries_) sorted_.push_back(e.instance);
    return sorted_;
}

void appendSceneSprites(const Scene2D& scene, SpriteBatch& batch, const AtlasLookup& lookup) {
    for (const auto& object : scene.getObjects()) {
        if (!object || !object->active || !object->visible || !object->sprite) continue;
        const SpriteData& sprite = *object->sprite;

        auto [material, uvRect] = lookup(sprite.atlasTextureIndex, sprite.frameName);
        if (uvRect.z <= 0.0f) continue;

        SpriteInstance inst{};
        inst.position = object->transform.position;
        inst.size = object->transform.scale;
        inst.uvRect = uvRect;
        inst.color = sprite.color;
        inst.rotation = object->transform.rotation;
        inst.depth = sprite.depth;
        inst.parallax = sprite.parallax;
        inst.albedo = material.albedo;
        inst.normal = material.normal;
        inst.emissive = material.emissive;
        inst.mask = material.mask;
        inst.flags = SpriteFlagNearest; // scene atlases are pixel art

        // Legacy `layer` only orders sprites that sit on the default depth.
        int order = sprite.depth == 0.0f ? sprite.layer * 1000 + sprite.orderInLayer : sprite.orderInLayer;
        batch.add(inst, order);
    }
}
