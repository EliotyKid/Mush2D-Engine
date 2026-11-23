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
        bool addA = (a.instance.flags & SpriteFlagAdditive) != 0, addB = (b.instance.flags & SpriteFlagAdditive) != 0;
        if (addA != addB) return addB; // opaque before additive
        if (a.instance.depth != b.instance.depth) return a.instance.depth > b.instance.depth;
        return a.order < b.order;
    });
    sorted_.clear();
    opaqueCount_ = 0;
    for (const Entry& e : entries_) {
        sorted_.push_back(e.instance);
        if (!(e.instance.flags & SpriteFlagAdditive)) ++opaqueCount_;
    }
    return sorted_;
}

void appendSceneSprites(const Scene2D& scene, SpriteBatch& batch, const AtlasLookup& lookup) {
    scene.forEach([&](const GameObject& obj) {
        const GameObject* object = &obj;
        if (!object->active || !object->visible || !object->sprite) return;
        const SpriteData& sprite = *object->sprite;

        auto [material, uvRect] = lookup(sprite.atlasTextureIndex, sprite.frameName);
        if (uvRect.z <= 0.0f) return;

        SpriteInstance inst{};
        const Transform2D world = object->world();
        // shortcut: sprite.offset is not rotated or scaled by the world transform; rotate/scale it here if
        // an object with an offset ever needs to turn or grow.
        inst.position = world.position + sprite.offset;
        inst.size = world.scale;
        inst.uvRect = uvRect;
        inst.color = sprite.color;
        inst.rotation = world.rotation;
        inst.depth = sprite.depth;
        inst.parallax = sprite.parallax;
        inst.albedo = material.albedo;
        inst.normal = material.normal;
        inst.emissive = material.emissive;
        inst.mask = material.mask;
        inst.flags = SpriteFlagNearest | (sprite.additive ? SpriteFlagAdditive : 0u); // scene atlases are pixel art

        // Legacy `layer` only orders sprites that sit on the default depth.
        int order = sprite.depth == 0.0f ? sprite.layer * 1000 + sprite.orderInLayer : sprite.orderInLayer;
        batch.add(inst, order);
    });
}
