#pragma once

#include "engine/assets/AssetRegistry.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/Prefab.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/SpriteAnimator.hpp"

#include <cstdio>

// Decor sprite (tree, bush, vine) from assets/prefabs/prop.prefab.json. This class only maps the LDtk entity:
// its `frame` field picks the frame of the sprite's atlas, standing with the frame pivot on the entity box's
// base (mirrored by flipX); `depth` is the GameObject attribute. Foreground props (depth < 0) take
// foregroundColor, dark silhouettes as in Ori.
class PropObject : public GameObject {
    MUSH2D_OBJECT(PropObject, GameObject)
public:
    MUSH2D_EXPORT(AssetRef, frame, "")
    MUSH2D_EXPORT(Color, foregroundColor, Color(0.32f, 0.36f, 0.42f, 1.0f))

    void onCreate(Scene2D&) override { name = "Prop"; }

    void onPrefabApplied(Scene2D& scene) override {
        const AssetRegistry* assets = scene.service<AssetRegistry>();
        const TextureAtlas* atlas = assets && sprite ? assets->atlas(sprite->atlasName) : nullptr;
        if (!atlas || !atlas->hasFrame(frame)) {
            std::fprintf(stderr, "Prop: frame '%s' not found, ignored\n", frame.c_str());
            scene.destroy(id);
            return;
        }
        sprite->frameName = frame;
        if (sprite->depth < 0.0f) sprite->color = foregroundColor;
        transform.position = ldtkBoxBottom(*this);
        placeFrameAtPivot(*this, atlas->getFrame(frame), {0.0f, 0.0f}, flipX);
    }
};
