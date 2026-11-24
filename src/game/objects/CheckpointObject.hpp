#pragma once

#include "game/assets/GameAssets.hpp"
#include "game/assets/GameSpriteId.hpp"

#include "engine/world/GameObject.hpp"
#include "engine/world/IPlayer.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"

// Touching it sets the player's checkpoint and, for an LDtk checkpoint (entityIid), records its activation
// time in the WorldState service: the latest activated one is the respawn point.
// Prefab: assets/prefabs/checkpoint.prefab.json.
class CheckpointObject : public GameObject {
    MUSH2D_OBJECT(CheckpointObject, GameObject)
public:
    MUSH2D_EXPORT(bool, oneShot, false)

    void onCreate(Scene2D& scene) override {
        name = "Checkpoint";
        addTag("trigger");
        addTag("checkpoint");

        sprite = SpriteData{.layer = 1, .orderInLayer = 1};
        if (GameAssets* assets = gameAssetsOf(scene)) {
            const auto& entry = assets->sprites.get(GameSpriteId::Checkpoint);
            sprite->atlasTextureIndex = entry.atlasTextureIndex;
            sprite->frameName = entry.frameName;
        }

        collider = ColliderData{.size = {1.2f, 1.2f}, .isTrigger = true};
        trigger = TriggerData{.type = TriggerType::Checkpoint};
    }

    void onTriggerEnter(Scene2D& scene, GameObject& other) override {
        auto* player = other.hasTag("player") ? dynamic_cast<IPlayer*>(&other) : nullptr;
        if (!player || !trigger.has_value() || (oneShot && trigger->consumed)) {
            return;
        }

        player->setCheckpoint(other.transform.position);
        WorldState* state = scene.service<WorldState>();
        if (state && !entityIid.empty()) state->set(entityIid, "activated", state->clock);

        if (oneShot) {
            trigger->consumed = true;
        }
    }
};
