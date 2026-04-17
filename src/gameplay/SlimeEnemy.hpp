#pragma once

#include "../world/GameObject.hpp"
#include "../world/Scene2D.hpp"

#include <cmath>

class SlimeEnemy : public GameObject {
public:
    struct Config {
        float moveSpeed = 1.0f;
        float patrolDistance = 2.0f;
        int maxHealth = 10;

        uint32_t textureIndex = 0;
        glm::vec2 colliderSize{0.8f, 0.8f};
        int layer = 1;
        int orderInLayer = 0;

        glm::vec2 initialScale{0.8f, 0.8f};
    };

public:
    SlimeEnemy() = default;
    explicit SlimeEnemy(const Config& config)
        : config(config) {}

    void onCreate(Scene2D& scene) override {
        name = "SlimeEnemy";
        addTag("enemy");
        addTag("slime");

        transform.scale = config.initialScale;

        sprite = SpriteData{
            .textureIndex = config.textureIndex,
            .layer = config.layer,
            .orderInLayer = config.orderInLayer,
            .color = glm::vec4(1.0f)
        };

        collider = ColliderData{
            .size = config.colliderSize,
            .offset = {0.0f, 0.0f},
            .isTrigger = false,
            .enabled = true
        };

        health = HealthData{
            .current = config.maxHealth,
            .max = config.maxHealth
        };

        moveSpeed = config.moveSpeed;
        patrolDistance = config.patrolDistance;
        spawnPosition = transform.position;
    }

    void onStep(Scene2D& scene, float dt) override {
        transform.position.x += direction * moveSpeed * dt;

        if (transform.position.x > spawnPosition.x + patrolDistance) {
            direction = -1;
        }

        if (transform.position.x < spawnPosition.x - patrolDistance) {
            direction = 1;
        }

        if (direction < 0) {
            transform.scale.x = -std::abs(transform.scale.x);
        } else {
            transform.scale.x = std::abs(transform.scale.x);
        }
    }

private:
    Config config{};

    float moveSpeed = 1.0f;
    float patrolDistance = 2.0f;
    glm::vec2 spawnPosition{0.0f, 0.0f};
    int direction = 1;
};