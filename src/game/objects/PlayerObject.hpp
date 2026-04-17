#pragma once

#include "../assets/GameAnimationId.hpp"
#include "../assets/GameAssets.hpp"
#include "../assets/GameSpriteId.hpp"

#include "../../engine/world/Collision.hpp"
#include "../../engine/world/GameObject.hpp"
#include "../../engine/world/Scene2D.hpp"
#include "../../engine/platform/InputState.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

class PlayerObject : public GameObject {
public:
    struct Config {
        float moveSpeed = 2.5f;
        float dashSpeed = 6.5f;
        float dashDuration = 0.18f;
        float hitDuration = 0.35f;

        glm::vec2 colliderSize{0.6f, 0.6f};
        int layer = 2;
        int orderInLayer = 0;

        glm::vec2 initialScale{0.7f, 0.7f};
    };

    enum class State {
        Idle,
        Walk,
        Dash,
        Hit
    };

public:
    PlayerObject() = default;
    explicit PlayerObject(const Config& config)
        : config(config) {}

    void onCreate(Scene2D& scene) override {
        name = "Player";
        addTag("player");

        transform.scale = config.initialScale;

        sprite = SpriteData{
            .atlasTextureIndex = 0,
            .frameName = "",
            .layer = config.layer,
            .orderInLayer = config.orderInLayer,
            .color = glm::vec4(0.85f, 0.85f, 0.85f, 1.0f)
        };

        collider = ColliderData{
            .size = config.colliderSize,
            .offset = {0.0f, 0.0f},
            .isTrigger = false,
            .enabled = true
        };

        facingDir = {1.0f, 0.0f};
        state = State::Idle;
        previousState = State::Idle;
        stateTime = 0.0f;
        history.clear();
        history.push_back(State::Idle);

        checkpointPosition = transform.position;

        currentAnimation = GameAnimationId::PlayerIdle;
        currentFrameIndex = 0;
        currentFrameTimer = 0.0f;

        playAnimation(scene, GameAnimationId::PlayerIdle, true);
        applyCurrentAnimationFrame(scene);
    }

    void applyInput(const InputState& input) {
        inputDir = {0.0f, 0.0f};

        if (input.moveLeft.down) inputDir.x -= 1.0f;
        if (input.moveRight.down) inputDir.x += 1.0f;
        if (input.moveUp.down) inputDir.y += 1.0f;
        if (input.moveDown.down) inputDir.y -= 1.0f;

        if (glm::length(inputDir) > 0.0f) {
            inputDir = glm::normalize(inputDir);
            facingDir = inputDir;
        }

        if (input.dash.pressed) {
            dashRequested = true;
        }

        if (input.hit.pressed) {
            hitRequested = true;
        }
    }

    void onStep(Scene2D& scene, float dt) override {
        stateTime += dt;
        updateState();

        if (state == State::Hit) {
            velocity = glm::vec2(0.0f);
        } else if (state == State::Dash) {
            velocity = facingDir * config.dashSpeed;
        } else {
            velocity = inputDir * config.moveSpeed;
        }

        glm::vec2 currentPosition = transform.position;
        glm::vec2 desiredPosition = currentPosition + velocity * dt;

        glm::vec2 tryX{desiredPosition.x, currentPosition.y};
        if (!Collision2D::collidesAtPosition(scene, id, tryX)) {
            transform.position.x = tryX.x;
        }

        glm::vec2 tryY{transform.position.x, desiredPosition.y};
        if (!Collision2D::collidesAtPosition(scene, id, tryY)) {
            transform.position.y = tryY.y;
        }

        if (facingDir.x < 0.0f) {
            transform.scale.x = -std::abs(transform.scale.x);
        } else if (facingDir.x > 0.0f) {
            transform.scale.x = std::abs(transform.scale.x);
        }

        updateVisuals(scene, dt);
    }

public:
    glm::vec2 velocity{0.0f};
    glm::vec2 facingDir{1.0f, 0.0f};

    State getState() const {
        return state;
    }

    State getPreviousState() const {
        return previousState;
    }

    float getStateTime() const {
        return stateTime;
    }

    const std::vector<State>& getHistory() const {
        return history;
    }

    void setCheckpointPosition(const glm::vec2& position) {
        checkpointPosition = position;
    }

    glm::vec2 getCheckpointPosition() const {
        return checkpointPosition;
    }

    void respawnAtCheckpoint() {
        transform.position = checkpointPosition;
    }

private:
    Config config{};

    State state = State::Idle;
    State previousState = State::Idle;
    float stateTime = 0.0f;

    glm::vec2 inputDir{0.0f};
    bool dashRequested = false;
    bool hitRequested = false;

    glm::vec2 checkpointPosition{0.0f, 0.0f};

    std::vector<State> history;

    GameAnimationId currentAnimation = GameAnimationId::PlayerIdle;
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

    void changeState(State newState) {
        previousState = state;
        state = newState;
        stateTime = 0.0f;
        history.push_back(newState);
    }

    void updateState() {
        if (hitRequested && state != State::Hit) {
            hitRequested = false;
            changeState(State::Hit);
            return;
        }

        if (dashRequested && state != State::Dash && state != State::Hit) {
            dashRequested = false;
            changeState(State::Dash);
            return;
        }

        switch (state) {
            case State::Idle:
            case State::Walk: {
                if (glm::length(inputDir) > 0.001f) {
                    if (state != State::Walk) {
                        changeState(State::Walk);
                    }
                } else {
                    if (state != State::Idle) {
                        changeState(State::Idle);
                    }
                }
                break;
            }

            case State::Dash: {
                if (stateTime >= config.dashDuration) {
                    if (glm::length(inputDir) > 0.001f) {
                        changeState(State::Walk);
                    } else {
                        changeState(State::Idle);
                    }
                }
                break;
            }

            case State::Hit: {
                if (stateTime >= config.hitDuration) {
                    changeState(State::Idle);
                }
                break;
            }
        }
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
        if (!sprite.has_value()) {
            return;
        }

        GameAnimationId desiredAnimation = GameAnimationId::PlayerIdle;

        switch (state) {
            case State::Idle:
                desiredAnimation = GameAnimationId::PlayerIdle;
                sprite->color = glm::vec4(0.85f, 0.85f, 0.85f, 1.0f);
                break;

            case State::Walk:
                desiredAnimation = GameAnimationId::PlayerWalk;
                sprite->color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                break;

            case State::Dash:
                desiredAnimation = GameAnimationId::PlayerDash;
                sprite->color = glm::vec4(0.6f, 0.9f, 1.0f, 1.0f);
                break;

            case State::Hit:
                desiredAnimation = GameAnimationId::PlayerHit;
                sprite->color = glm::vec4(1.0f, 0.45f, 0.45f, 1.0f);
                break;
        }

        playAnimation(scene, desiredAnimation, false);
        updateAnimation(scene, dt);
    }
};