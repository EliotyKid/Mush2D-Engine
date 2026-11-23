#pragma once

#include "game/assets/GameAnimationId.hpp"
#include "game/assets/GameAssets.hpp"
#include "game/assets/GameSpriteId.hpp"
#include "game/objects/Tracked.hpp"

#include "engine/world/CharacterController.hpp"
#include "engine/world/Collision.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

class SlimeEnemy : public GameObject {
    MUSH2D_OBJECT(SlimeEnemy, GameObject)
public:
    struct Config {
        float moveSpeed = 1.0f;
        float patrolDistance = 2.0f;
        int maxHealth = 10;

        glm::vec2 colliderSize{0.8f, 0.8f};
        int layer = 1;
        int orderInLayer = 0;

        glm::vec2 initialScale{0.8f, 0.8f};
        WorldState* worldState = nullptr; // records the kill (PersistentComponent) when set
        float gravity = -32.0f; // 0 in top-down scenes
        float maxFallSpeed = -18.0f;
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

        currentAnimation = GameAnimationId::SlimeIdle;
        currentFrameIndex = 0;
        currentFrameTimer = 0.0f;

        playAnimation(scene, GameAnimationId::SlimeIdle, true);
        applyCurrentAnimationFrame(scene);
    }

    void onStep(Scene2D& scene, float dt) override {
        // The patrol centre: spawners place the slime after onCreate, so take it at the first step.
        if (!patrolAnchored) { spawnPosition = transform.position; patrolAnchored = true; }
        // Walks with the controller, ignoring the player (contact is a trigger, plus the stomp).
        body.position = transform.position + collider->offset;
        body.halfSize = collider->size * 0.5f;
        velocity.x = direction * moveSpeed;
        velocity.y = std::max(velocity.y + config.gravity * dt, config.maxFallSpeed);
        const float reach = glm::length(velocity) * dt + 1.0f;
        const Collision2D::AABB area{body.position - body.halfSize - reach, body.position + body.halfSize + reach};
        const CollisionQuery query{&scene.collision, Collision2D::solidBoxes(scene, id, area, "player")};
        const MoveResult r = move(body, velocity, dt, query, grounded, false);
        transform.position = r.position - collider->offset;
        velocity = r.velocity;
        grounded = config.gravity != 0.0f && r.grounded; // top-down slimes are never grounded

        // Turn at a wall or a steep surface (they stop v.x) or at a ledge: a copy of the body walked one body
        // width ahead, 0.1 u at a time, loses the ground (the controller snaps it down slopes up to 0.3 u).
        bool turn = moveSpeed != 0.0f && r.velocity.x * float(direction) <= 0.0f;
        if (grounded && moveSpeed != 0.0f && !turn) {
            Body ahead = body;
            for (float walked = 0.0f; walked < 2.0f * body.halfSize.x + 0.05f && !turn; walked += 0.1f)
                turn = !move(ahead, {float(direction) * 0.1f, 0.0f}, 1.0f, query, true, false).grounded;
        }
        if (turn) direction = -direction;

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

    // The player landed on it (PlayerObject decides when): dies.
    void stomp(Scene2D& scene) {
        if (config.worldState) removeTracked(scene, *config.worldState, *this, config.worldState->clock);
        else scene.destroy(id);
    }

private:
    Config config{};

    float moveSpeed = 1.0f;
    float patrolDistance = 2.0f;
    glm::vec2 spawnPosition{0.0f, 0.0f};
    bool patrolAnchored = false;
    int direction = 1;
    glm::vec2 velocity{0.0f};
    bool grounded = false;
    Body body;

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
        if (!gameAssetsOf(scene)) {
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
        if (!gameAssetsOf(scene)) {
            return;
        }

        const auto& animationEntry = gameAssetsOf(scene)->animations.get(currentAnimation);
        const TextureAtlas& atlas = gameAssetsOf(scene)->getAtlasByTextureIndex(animationEntry.atlasTextureIndex);

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
        if (!gameAssetsOf(scene)) {
            return;
        }

        const auto& animationEntry = gameAssetsOf(scene)->animations.get(currentAnimation);
        const TextureAtlas& atlas = gameAssetsOf(scene)->getAtlasByTextureIndex(animationEntry.atlasTextureIndex);

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