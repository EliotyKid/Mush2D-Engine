#pragma once

#include "iki_demo/assets/GameAssets.hpp"
#include "iki_demo/assets/GameSpriteId.hpp"

#include "engine/world/GameObject.hpp"
#include "engine/world/IPlayer.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"

// Trigger: while the player stands on it, pressing down toggles the lever ("on", under its entityIid) and its
// door ("open"), in the WorldState service. Prefab: assets/prefabs/lever.prefab.json; LDtk field `door`.
class LeverObject : public GameObject {
    MUSH2D_OBJECT(LeverObject, GameObject)
public:
    MUSH2D_EXPORT(EntityRef, door, "") // the door it opens (LDtk entity iid)

    void onCreate(Scene2D& scene) override {
        state = scene.service<WorldState>();
        name = "Lever";
        addTag("trigger");
        addTag("lever");
        transform.scale = {0.6f, 0.6f};
        sprite = SpriteData{.layer = 1, .orderInLayer = 2};
        if (GameAssets* assets = gameAssetsOf(scene)) {
            const auto& entry = assets->sprites.get(GameSpriteId::Teleport);
            sprite->atlasTextureIndex = entry.atlasTextureIndex;
            sprite->frameName = entry.frameName;
        }
        collider = ColliderData{.size = {1.0f, 1.0f}, .isTrigger = true};
        refresh();
    }

    void onPrefabApplied(Scene2D&) override { refresh(); }

    void onStep(Scene2D& scene, float) override {
        auto* player = scene.findObjectAs<IPlayer>(playerId);
        if (player && player->downPressedThisFrame()) toggle();
        refresh();
    }

    void onTriggerEnter(Scene2D&, GameObject& other) override {
        if (other.hasTag("player")) playerId = other.id;
    }
    void onTriggerExit(Scene2D&, GameObject& other) override {
        if (other.id == playerId) playerId = InvalidGameObjectId;
    }

    void toggle() {
        if (!state) return;
        state->set(entityIid, "on", isOn() ? 0.0 : 1.0);
        if (!door.empty()) state->set(door, "open", state->get(door, "open", 0.0) != 0.0 ? 0.0 : 1.0);
    }
    bool isOn() const { return state && state->get(entityIid, "on", 0.0) != 0.0; }

private:
    void refresh() { sprite->color = isOn() ? glm::vec4(0.4f, 1.0f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.4f, 0.4f, 1.0f); }

    WorldState* state = nullptr;
    GameObjectId playerId = InvalidGameObjectId;
};
