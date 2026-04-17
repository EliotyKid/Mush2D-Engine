#pragma once

#include "../assets/GameAnimationId.hpp"
#include "../assets/GameAssets.hpp"
#include "../assets/GameSpriteId.hpp"

#include "../../engine/world/GameObject.hpp"
#include "../../engine/world/Scene2D.hpp"

#include <cmath>
#include <cstddef>

class SlimeEnemy : public GameObject {
public:
    struct Config {
        float moveSpeed = 1.0f;
        float patrolDistance = 2.0f;
        int maxHealth = 10;

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
            .atlasTextureIndex = 0,
            .frameName = "",
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

        currentAnimation = GameAnimationId::SlimeIdle;
        currentFrameIndex = 0;
        currentFrameTimer = 0.0f;

        playAnimation(scene, GameAnimationId::SlimeIdle, true);
        applyCurrentAnimationFrame(scene);
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

        updateVisuals(scene, dt);
    }

private:
    Config config{};

    float moveSpeed = 1.0f;
    float patrolDistance = 2.0f;
    glm::vec2 spawnPosition{0.0f, 0.0f};
    int direction = 1;

    GameAnimationId currentAnimation = GameAnimationId::SlimeIdle;
    std::size_t currentFrameIndex = 0;
    float currentFrameTimer = 0.0f;

private:
    void setSpriteFrame(Scene2D& scene, std::uint32_t atlasTextureIndex, const std::string& frameName) {
        if (!sprite.has_value()) {
            return;
        }

        sprite->atlasTextureIndex = atlasTextureIndex;
        sprite->frameName = frameName;
    }

    void playAnimation(Scene2D& scene, GameAnimationId animationId, bool restart = false) {
        if (!scene.gameAssets) {
            return;
        }

        if (!restart && currentAnimation == animationId) {
            return;
        }

        currentAnimation = animationId;
        currentFrameIndex = 0;
        currentFrameTimer = 0.0f;
        applyCurrentAnimationFrame(scene);
    }

    void applyCurrentAnimationFrame(Scene2D& scene) {
        if (!scene.gameAssets) {
            return;
        }

        const auto& animationEntry = scene.gameAssets->animations.get(currentAnimation);
        const TextureAtlas& atlas = scene.gameAssets->getAtlasByTextureIndex(animationEntry.atlasTextureIndex);

        if (!atlas.hasAnimation(animationEntry.animationName)) {
            return;
        }

        const AtlasAnimation& animation = atlas.getAnimation(animationEntry.animationName);
        if (animation.frames.empty()) {
            return;
        }

        if (currentFrameIndex >= animation.frames.size()) {
            currentFrameIndex = 0;
        }

        const AtlasAnimationFrame& frame = animation.frames[currentFrameIndex];
        setSpriteFrame(scene, animationEntry.atlasTextureIndex, frame.frameName);
    }

    void updateAnimation(Scene2D& scene, float dt) {
        if (!scene.gameAssets) {
            return;
        }

        const auto& animationEntry = scene.gameAssets->animations.get(currentAnimation);
        const TextureAtlas& atlas = scene.gameAssets->getAtlasByTextureIndex(animationEntry.atlasTextureIndex);

        if (!atlas.hasAnimation(animationEntry.animationName)) {
            return;
        }

        const AtlasAnimation& animation = atlas.getAnimation(animationEntry.animationName);
        if (animation.frames.empty()) {
            return;
        }

        if (currentFrameIndex >= animation.frames.size()) {
            currentFrameIndex = 0;
        }

        currentFrameTimer += dt;

        while (true) {
            const AtlasAnimationFrame& frame = animation.frames[currentFrameIndex];
            float frameDurationSeconds = static_cast<float>(frame.durationMs) / 1000.0f;

            if (frameDurationSeconds <= 0.0f) {
                frameDurationSeconds = 0.1f;
            }

            if (currentFrameTimer < frameDurationSeconds) {
                break;
            }

            currentFrameTimer -= frameDurationSeconds;

            if (currentFrameIndex + 1 < animation.frames.size()) {
                currentFrameIndex++;
            } else {
                if (animation.looping) {
                    currentFrameIndex = 0;
                } else {
                    currentFrameIndex = animation.frames.size() - 1;
                }
            }

            applyCurrentAnimationFrame(scene);

            if (!animation.looping && currentFrameIndex == animation.frames.size() - 1) {
                break;
            }
        }
    }

    void updateVisuals(Scene2D& scene, float dt) {
        GameAnimationId desiredAnimation =
            (std::abs(moveSpeed) > 0.001f)
            ? GameAnimationId::SlimeWalk
            : GameAnimationId::SlimeIdle;

        playAnimation(scene, desiredAnimation, false);
        updateAnimation(scene, dt);
    }
};