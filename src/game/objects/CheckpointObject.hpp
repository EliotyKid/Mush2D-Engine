#pragma once

#include "PlayerObject.hpp"

#include "../../engine/world/GameObject.hpp"
#include "../../engine/world/Scene2D.hpp"

class CheckpointObject : public GameObject {
public:
    struct Config {
        uint32_t textureIndex = 1;
        int layer = 1;
        int orderInLayer = 1;

        glm::vec2 triggerSize{1.2f, 1.2f};
        bool oneShot = false;

        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
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
            .textureIndex = config.textureIndex,
            .layer = config.layer,
            .orderInLayer = config.orderInLayer,
            .color = config.color
        };

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

            if (trigger->oneShot) {
                trigger->consumed = true;
            }
        }
    }

private:
    Config config{};
};