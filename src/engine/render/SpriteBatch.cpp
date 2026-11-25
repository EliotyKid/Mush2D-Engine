#include "engine/render/SpriteBatch.hpp"

#include "engine/world/Particles.hpp"
#include "engine/world/Scene2D.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>

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

namespace {
void appendParticles(const GameObject& owner, SpriteBatch& batch, const AtlasLookup& lookup, const Material* white) {
    for (const auto& component : owner.components()) {
        const auto* emitter = dynamic_cast<const ParticleEmitter*>(component.get());
        if (!emitter || !emitter->enabled || emitter->removing()) continue;
        for (const Particle& p : emitter->particles()) {
            const ParticleConfig& c = *p.config;
            Material material;
            glm::vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
            if (c.frame.empty()) {
                if (!white) continue;
                material = *white;
            } else {
                // shortcut: the frame comes from the owner's sprite atlas; add an "atlas/frame" ref if effects need others.
                std::tie(material, uvRect) = lookup(owner.sprite ? owner.sprite->atlasTextureIndex : 0, c.frame);
                if (uvRect.z <= 0.0f) continue;
            }
            const float t = p.life > 0.0f ? std::min(p.age / p.life, 1.0f) : 1.0f;
            const float size = c.size.x + (c.size.y - c.size.x) * t;
            SpriteInstance inst{};
            inst.position = p.position;
            inst.size = {size, size};
            inst.uvRect = uvRect;
            inst.color = c.colorStart + (c.colorEnd - c.colorStart) * t;
            inst.parallax = {1.0f, 1.0f};
            inst.albedo = material.albedo;
            inst.normal = material.normal;
            inst.emissive = material.emissive;
            inst.mask = material.mask;
            inst.flags = c.additive ? SpriteFlagAdditive : SpriteFlagNearest;
            batch.add(inst, c.layer * 1000);
        }
    }
}
}

void appendSceneSprites(const Scene2D& scene, SpriteBatch& batch, const AtlasLookup& lookup, const Material* white) {
    scene.forEach([&](const GameObject& obj) {
        const GameObject* object = &obj;
        if (object->active) appendParticles(*object, batch, lookup, white); // particles outlive a hidden owner's blink
        if (!object->active || !object->visible || !object->sprite) return;
        const SpriteData& sprite = *object->sprite;

        auto [material, uvRect] = lookup(sprite.atlasTextureIndex, sprite.frameName);
        if (uvRect.z <= 0.0f) return;

        SpriteInstance inst{};
        const Transform2D world = object->world();
        // The offset grows and mirrors with the scale, so a pivot stays put on a scaled object, and turns with
        // the rotation, so a sprite turns around its pivot (VelocityLean leans around the feet).
        const glm::vec2 off = world.scale * sprite.offset;
        const float c = std::cos(world.rotation), s = std::sin(world.rotation);
        inst.position = world.position + glm::vec2(c * off.x - s * off.y, s * off.x + c * off.y);
        inst.size = sprite.drawSize == glm::vec2(0.0f) ? world.scale : world.scale * sprite.drawSize;
        inst.uvRect = uvRect;
        inst.color = sprite.color * sprite.tint;
        inst.rotation = world.rotation;
        inst.depth = sprite.depth;
        inst.parallax = sprite.parallax;
        inst.albedo = material.albedo;
        inst.normal = material.normal;
        inst.emissive = material.emissive;
        inst.mask = material.mask;
        // Scene atlases are pixel art (nearest); additive glows are soft (linear), as the old crystal halos.
        inst.flags = sprite.additive ? SpriteFlagAdditive : SpriteFlagNearest;

        // Legacy `layer` only orders sprites that sit on the default depth.
        int order = sprite.depth == 0.0f ? sprite.layer * 1000 + sprite.orderInLayer : sprite.orderInLayer;
        batch.add(inst, order);
    });
}
