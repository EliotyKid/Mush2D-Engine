#pragma once

#include "../../engine/world/GameObject.hpp"
#include "../../engine/world/Scene2D.hpp"

class WallObject : public GameObject {
public:
    struct Config {
        uint32_t textureIndex = 0;
        int layer = 0;
        int orderInLayer = 0;

        glm::vec2 size{1.0f, 1.0f};
        glm::vec2 colliderOffset{0.0f, 0.0f};
    };

public:
    WallObject() = default;
    explicit WallObject(const Config& config)
        : config(config) {}

    void onCreate(Scene2D& scene) override {
        name = "Wall";
        addTag("wall");

        sprite = SpriteData{
            .textureIndex = config.textureIndex,
            .layer = config.layer,
            .orderInLayer = config.orderInLayer,
            .color = glm::vec4(1.0f)
        };

        collider = ColliderData{
            .size = config.size,
            .offset = config.colliderOffset,
            .isTrigger = false,
            .enabled = true
        };
    }

private:
    Config config{};
};