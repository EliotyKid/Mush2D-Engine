#pragma once

#include "game/assets/GameAssets.hpp"
#include "game/assets/GameSpriteId.hpp"
#include "game/objects/PlayerObject.hpp"
#include "game/objects/Tracked.hpp"

#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"

#include <string>

// Collected on touch. No inventory yet: collecting only records the state (persistence policy) in the
// WorldState service. Prefab: assets/prefabs/item.prefab.json; LDtk field `kind`.
class ItemObject : public GameObject {
    MUSH2D_OBJECT(ItemObject, GameObject)
public:
    MUSH2D_EXPORT(std::string, kind, "")

    void onCreate(Scene2D& scene) override {
        name = "Item";
        addTag("trigger");
        addTag("item");
        transform.scale = {0.5f, 0.5f};
        sprite = SpriteData{.layer = 1, .orderInLayer = 3, .color = glm::vec4(3.0f, 2.2f, 0.6f, 1.0f)};
        if (GameAssets* assets = gameAssetsOf(scene)) {
            const auto& entry = assets->sprites.get(GameSpriteId::Teleport);
            sprite->atlasTextureIndex = entry.atlasTextureIndex;
            sprite->frameName = entry.frameName;
        }
        collider = ColliderData{.size = {0.6f, 0.6f}, .isTrigger = true};
    }

    void onTriggerEnter(Scene2D& scene, GameObject& other) override {
        if (!dynamic_cast<PlayerObject*>(&other)) return;
        if (WorldState* state = scene.service<WorldState>()) removeTracked(scene, *state, *this, state->clock);
        else scene.destroy(id);
    }
};
