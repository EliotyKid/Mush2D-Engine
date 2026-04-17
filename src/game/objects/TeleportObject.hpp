#pragma once

#include "../assets/GameAssets.hpp"
#include "../assets/GameSpriteId.hpp"

#include "PlayerObject.hpp"

#include "../../engine/world/GameObject.hpp"
#include "../../engine/world/Scene2D.hpp"

class TeleportObject : public GameObject {
public:
    struct Config {
        int layer = 1;
        int orderInLayer = 2;

        glm::vec2 triggerSize{1.2f, 1.2f};
        glm::vec2 targetPosition{0.0f, 0.0f};

        bool oneShot = false;
        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    };

public:
    TeleportObject() = default;
    explicit TeleportObject(const Config& config)
        : config(config) {}

    void onCreate(Scene2D& scene) override {
        name = "Teleport";
        addTag("trigger");
        addTag("teleport");

        sprite = SpriteData{
            .atlasTextureIndex = 0,
            .frameName = "",
            .layer = config.layer,
            .orderInLayer = config.orderInLayer,
            .color = config.color
        };

        setSprite(scene, GameSpriteId::Teleport);

        collider = ColliderData{
            .size = config.triggerSize,
            .offset = {0.0f, 0.0f},
            .isTrigger = true,
            .enabled = true
        };

        trigger = TriggerData{
            .type = TriggerType::Teleport,
            .targetPosition = config.targetPosition,
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

            player->transform.position = trigger->targetPosition;

            if (trigger->oneShot) {
                trigger->consumed = true;
            }
        }
    }

private:
    Config config{};

private:
    void setSprite(Scene2D& scene, GameSpriteId spriteId) {
        if (!sprite.has_value() || !scene.gameAssets) {
            return;
        }

        const auto& entry = scene.gameAssets->sprites.get(spriteId);
        sprite->atlasTextureIndex = entry.atlasTextureIndex;
        sprite->frameName = entry.frameName;
    }
};