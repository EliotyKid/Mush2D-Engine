#pragma once

#include "game/assets/GameAssets.hpp"
#include "game/assets/GameSpriteId.hpp"

#include "PlayerObject.hpp"

#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"

#include <string>

class CheckpointObject : public GameObject {
public:
    struct Config {
        int layer = 1;
        int orderInLayer = 1;

        glm::vec2 triggerSize{1.2f, 1.2f};
        bool oneShot = false;

        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};

        WorldState* worldState = nullptr; // when set (and iid is), touching it records iid "activated" = clock
        std::string iid;
    };

public:
    CheckpointObject() = default;
    explicit CheckpointObject(const Config& config)
        : config(config) {}

    void onCreate(Scene2D& scene) override {
        name = "Checkpoint";
        addTag("trigger");
        addTag("checkpoint");

        sprite = SpriteData{
            .atlasTextureIndex = 0,
            .frameName = "",
            .layer = config.layer,
            .orderInLayer = config.orderInLayer,
            .color = config.color
        };

        setSprite(scene, GameSpriteId::Checkpoint);

        collider = ColliderData{
            .size = config.triggerSize,
            .offset = {0.0f, 0.0f},
            .isTrigger = true,
            .enabled = true
        };

        trigger = TriggerData{
            .type = TriggerType::Checkpoint,
            .targetPosition = {0.0f, 0.0f},
            .oneShot = config.oneShot,
            .consumed = false
        };
    }

    void onTriggerEnter(Scene2D& scene, GameObject& other) override {
        auto* player = dynamic_cast<PlayerObject*>(&other);
        if (!player) {
            return;
        }

        if (trigger.has_value()) {
            if (trigger->oneShot && trigger->consumed) {
                return;
            }

            player->setCheckpointPosition(player->transform.position);
            // Activation time: the latest activated checkpoint is the respawn point.
            if (config.worldState && !config.iid.empty())
                config.worldState->set(config.iid, "activated", config.worldState->clock);

            if (trigger->oneShot) {
                trigger->consumed = true;
            }
        }
    }

private:
    Config config{};

private:
    void setSprite(Scene2D& scene, GameSpriteId spriteId) {
        if (!sprite.has_value() || !gameAssetsOf(scene)) {
            return;
        }

        const auto& entry = gameAssetsOf(scene)->sprites.get(spriteId);
        sprite->atlasTextureIndex = entry.atlasTextureIndex;
        sprite->frameName = entry.frameName;
    }
};