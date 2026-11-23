#pragma once

#include "engine/world/GameObject.hpp"
#include "engine/world/Prefab.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/SpriteAnimator.hpp"

// Decor animal: a sprite and a SpriteAnimator from assets/prefabs/critter.prefab.json. This class only maps
// the LDtk entity onto them: `anim` (field of that name) becomes the "idle" animation, and the feet stand on
// the entity box's base. `flipX` is the GameObject attribute.
class CritterObject : public GameObject {
    MUSH2D_OBJECT(CritterObject, GameObject)
public:
    MUSH2D_EXPORT(AssetRef, anim, "") // "idle" animation (name in the sprite's atlas); empty = the prefab's

    void onPrefabApplied(Scene2D&) override {
        transform.position = ldtkBoxBottom(*this);
        if (anim.empty()) return;
        SpriteAnimator* animator = getComponent<SpriteAnimator>();
        if (!animator) animator = &addComponent<SpriteAnimator>();
        animator->animations["idle"] = anim;
    }
};
