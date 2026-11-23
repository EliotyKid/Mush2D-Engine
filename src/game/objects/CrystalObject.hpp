#pragma once

#include "engine/assets/AssetRegistry.hpp"
#include "engine/render/LightComponent.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/Prefab.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/SpriteAnimator.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

// Glowing crystal from assets/prefabs/crystal.prefab.json: an emissive sprite, a "Halo" child drawn in the
// additive pass (crystal_halo.prefab.json) and a flickering light, all coloured by `variant` (LDtk field).
// The frame "crystal <variant>" stands with its pivot on the entity box's base.
class CrystalObject : public GameObject {
    MUSH2D_OBJECT(CrystalObject, GameObject)
public:
    MUSH2D_EXPORT(int, variant, 0) // 0..3: cyan, magenta, light cyan, pink

    void onCreate(Scene2D&) override { name = "Crystal"; }

    void onPrefabApplied(Scene2D& scene) override {
        static constexpr glm::vec3 Glow[4] = {{0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.9f, 1.0f}, {1.0f, 0.16f, 0.86f}};
        variant = std::clamp(variant, 0, 3);
        const std::string frame = "crystal " + std::to_string(variant);
        const AssetRegistry* assets = scene.service<AssetRegistry>();
        const TextureAtlas* atlas = assets && sprite ? assets->atlas(sprite->atlasName) : nullptr;
        if (!atlas || !atlas->hasFrame(frame)) {
            std::fprintf(stderr, "Crystal: frame '%s' not found, ignored\n", frame.c_str());
            scene.destroy(id);
            return;
        }
        sprite->frameName = frame;
        const AtlasFrame& f = atlas->getFrame(frame);
        transform.position = ldtkBoxBottom(*this) + glm::vec2(0.0f, float(f.pivotY) - 0.5f * float(f.h)) / ArtPixelsPerUnit;

        const glm::vec3 glow = Glow[variant];
        for (GameObjectId childId : children())
            if (GameObject* child = scene.find(childId); child && child->name == "Halo" && child->sprite)
                child->sprite->color = glm::vec4(glow * 0.45f, 1.0f);
        LightComponent* light = getComponent<LightComponent>();
        if (!light) light = &addComponent<LightComponent>();
        light->light = GpuLight{{0.0f, 0.0f, 0.6f, 3.0f}, {glow, 1.4f}, {0.0f, 0.06f, 2.0f, 0.0f}};
    }
};
