#pragma once

#include "iki_demo/assets/GameAnimationId.hpp"
#include "iki_demo/assets/GameAssets.hpp"
#include "iki_demo/assets/GameSpriteId.hpp"
#include "engine/world/SpriteAnimator.hpp"
#include "iki_demo/objects/SlimeEnemy.hpp"

#include "engine/world/CharacterController.hpp"
#include "engine/world/Collision.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/IPlayer.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/platform/Actions.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

MUSH2D_ENUM(PlayerMovementMode, TopDown, Platformer)

// The player keeps its own animation code until D2; its movement values are exported attributes (the
// inspector edits them live). Config is the code way to set them all at creation (scenes, PlayerStart).
class PlayerObject : public GameObject, public IPlayer {
    MUSH2D_OBJECT(PlayerObject, GameObject)
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

        using MovementMode = PlayerMovementMode;
        MovementMode movementMode = MovementMode::TopDown;
        // Platformer only (world units / s). Releasing jump while rising multiplies velocity.y by jumpCutFactor.
        float gravity = -30.0f;
        float jumpSpeed = 11.0f;
        float jumpCutFactor = 0.5f;
        float maxFallSpeed = -18.0f;
    };

    enum class State {
        Idle,
        Walk,
        Dash,
        Hit
    };

    // Movement (same names and defaults as Config).
    MUSH2D_EXPORT(float, moveSpeed, 2.5f)
    MUSH2D_EXPORT(float, dashSpeed, 6.5f)
    MUSH2D_EXPORT(float, dashDuration, 0.18f)
    MUSH2D_EXPORT(float, hitDuration, 0.35f)
    MUSH2D_EXPORT(PlayerMovementMode, movementMode, PlayerMovementMode::TopDown)
    // Platformer only (world units / s). Releasing jump while rising multiplies velocity.y by jumpCutFactor.
    MUSH2D_EXPORT(float, gravity, -30.0f)
    MUSH2D_EXPORT(float, jumpSpeed, 11.0f)
    MUSH2D_EXPORT(float, jumpCutFactor, 0.5f)
    MUSH2D_EXPORT(float, maxFallSpeed, -18.0f)

public:
    PlayerObject() = default;
    explicit PlayerObject(const Config& config)
        : config(config) {
        moveSpeed = config.moveSpeed;
        dashSpeed = config.dashSpeed;
        dashDuration = config.dashDuration;
        hitDuration = config.hitDuration;
        movementMode = config.movementMode;
        gravity = config.gravity;
        jumpSpeed = config.jumpSpeed;
        jumpCutFactor = config.jumpCutFactor;
        maxFallSpeed = config.maxFallSpeed;
    }

    void onCreate(Scene2D& scene) override {
        name = "Player";
        addTag("player");

        // Platformer: the frame sizes the sprite (placeFrameAtPivot -> sprite.drawSize), times this scale.
        transform.scale = platformer() ? glm::vec2(1.0f) : config.initialScale;

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

        currentAnimation = idleAnimation();
        currentFrameIndex = 0;
        currentFrameTimer = 0.0f;

        if (platformer()) {
            sprite->color = glm::vec4(1.0f);
        }

        playAnimation(scene, idleAnimation(), true);
        applyCurrentAnimationFrame(scene);
    }

    // Reads the project's named actions (examples/iki_demo/project.json) for this frame.
    void applyInput(const ActionFrame& input) override {
        inputDir = {0.0f, 0.0f};

        if (input.down("moveLeft")) inputDir.x -= 1.0f;
        if (input.down("moveRight")) inputDir.x += 1.0f;
        if (input.down("moveUp")) inputDir.y += 1.0f;
        if (input.down("moveDown")) inputDir.y -= 1.0f;

        if (glm::length(inputDir) > 0.0f) {
            inputDir = glm::normalize(inputDir);
            facingDir = inputDir;
        }

        if (input.pressed("dash")) {
            dashRequested = true;
        }

#ifndef NDEBUG
        if (input.pressed("hit")) { // H: a test hit, Debug only
            hitRequested = true;
        }
#endif

        if (input.pressed("jump") || input.pressed("moveUp")) {
            jumpRequested = true;
        }
        jumpHeld = input.down("jump") || input.down("moveUp");
        interact = input.pressed("interact"); // levers
    }

    // Down was pressed this frame (levers read it during the step).
    bool interactPressed() const { return interact; }
    bool downPressedThisFrame() const override { return interact; }

    void onStep(Scene2D& scene, float dt) override {
        if (platformer()) {
            stepPlatformer(scene, dt);
            return;
        }

        stateTime += dt;
        updateState();

        if (state == State::Hit) {
            velocity = glm::vec2(0.0f);
        } else if (state == State::Dash) {
            velocity = facingDir * dashSpeed;
        } else {
            velocity = inputDir * moveSpeed;
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

    void setCheckpoint(glm::vec2 position) override { setCheckpointPosition(position); }
    void respawnAt(glm::vec2 position) override { teleport(position); }

    void respawnAtCheckpoint() override {
        teleport(checkpointPosition);
    }

    void teleport(const glm::vec2& position) {
        // Any teleport (respawn, death, hot reload): no ground, slide or drop-through carried over.
        transform.position = position;
        velocity = glm::vec2(0.0f);
        lastMove = {};
        grounded = false;
        body.droppingThrough = nullptr;
    }

    bool isGrounded() const {
        return grounded;
    }

private:
    Config config{};

    State state = State::Idle;
    State previousState = State::Idle;
    float stateTime = 0.0f;

    glm::vec2 inputDir{0.0f};
    bool dashRequested = false;
    bool hitRequested = false;
    bool jumpRequested = false;
    bool jumpHeld = false;
    bool jumpWasHeld = false;
    bool grounded = false;
    bool interact = false;
    Body body;           // platformer: the controller's body (drop-through state lives here)
    MoveResult lastMove; // platformer: last controller result (steep slide, one-way ground)

    glm::vec2 checkpointPosition{0.0f, 0.0f};

    std::vector<State> history;

    GameAnimationId currentAnimation = GameAnimationId::PlayerIdle;
    std::size_t currentFrameIndex = 0;
    float currentFrameTimer = 0.0f;

private:
    bool platformer() const {
        return movementMode == PlayerMovementMode::Platformer;
    }

    GameAnimationId idleAnimation() const {
        return platformer() ? GameAnimationId::KaaIdle : GameAnimationId::PlayerIdle;
    }

    // Landed while falling: a slime right below whose centre is under the player's feet dies, and the player
    // bounces (spec decision 1: stomp). Returns true when it bounced.
    bool stompBelow(Scene2D& scene) {
        Collision2D::AABB probe = Collision2D::buildAABB(*this);
        const float feet = probe.min.y;
        probe.min.y -= 0.05f;
        probe.max.y -= 0.05f;
        bool stomped = false;
        for (GameObjectId other : Collision2D::queryAABB(scene, probe, Collision2D::QueryKind::Solid)) {
            auto* slime = scene.findObjectAs<SlimeEnemy>(other);
            if (!slime || !Collision2D::hasCollider(*slime)) continue;
            const Collision2D::AABB box = Collision2D::buildAABB(*slime);
            if (!Collision2D::intersects(probe, box) || feet <= 0.5f * (box.min.y + box.max.y)) continue;
            slime->stomp(scene);
            stomped = true;
        }
        if (stomped) velocity.y = jumpSpeed * 0.6f;
        return stomped;
    }

    // The old stepPlatformer feel (input, jump, jump cut, gravity) driving the CharacterController.
    void stepPlatformer(Scene2D& scene, float dt) {
        body.position = transform.position + collider->offset;
        body.halfSize = collider->size * 0.5f;

        velocity.x = nextVelocityX(lastMove, inputDir.x * moveSpeed);
        bool jumped = false;
        if (jumpRequested && grounded) {
            // Down + jump on a one-way platform drops through it; anywhere else it is a normal jump.
            if (!(inputDir.y < 0.0f && requestDropThrough(body, lastMove))) {
                velocity.y = jumpSpeed;
                jumped = true;
            }
        }
        jumpRequested = false;
        if (jumpWasHeld && !jumpHeld && velocity.y > 0.0f) {
            velocity.y *= jumpCutFactor; // variable jump: once, on release
        }
        jumpWasHeld = jumpHeld;
        velocity.y = std::max(velocity.y + gravity * dt, maxFallSpeed);

        const float reach = glm::length(velocity) * dt + 1.0f;
        const Collision2D::AABB area{body.position - body.halfSize - reach, body.position + body.halfSize + reach};
        const CollisionQuery query{&scene.collision, Collision2D::solidBoxes(scene, id, area)};
        lastMove = move(body, velocity, dt, query, grounded, jumped);
        transform.position = lastMove.position - collider->offset;
        velocity = lastMove.velocity;
        grounded = lastMove.grounded;
        if (grounded && stompBelow(scene)) grounded = false; // the bounce leaves the ground

        if (inputDir.x != 0.0f) {
            facingDir = {inputDir.x > 0.0f ? 1.0f : -1.0f, 0.0f};
        }

        GameAnimationId desired = GameAnimationId::KaaIdle;
        if (!grounded) {
            desired = velocity.y > 0.0f ? GameAnimationId::KaaJump : GameAnimationId::KaaFall;
        } else if (inputDir.x != 0.0f) {
            desired = GameAnimationId::KaaRun;
        }
        playAnimation(scene, desired, false);
        updateAnimation(scene, dt);
        applyCurrentAnimationFrame(scene); // facing may have changed without a frame change
    }

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
                if (stateTime >= dashDuration) {
                    if (glm::length(inputDir) > 0.001f) {
                        changeState(State::Walk);
                    } else {
                        changeState(State::Idle);
                    }
                }
                break;
            }

            case State::Hit: {
                if (stateTime >= hitDuration) {
                    changeState(State::Idle);
                }
                break;
            }
        }
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

        if (platformer()) {
            // Frame pivot (feet) on the collider bottom; size from the frame.
            placeFrameAtPivot(*this, atlas.getFrame(frame.frameName), {0.0f, -0.5f * collider->size.y},
                              facingDir.x < 0.0f);
        }
    }

    bool holdsLastFrame(GameAnimationId id) const {
        return id == GameAnimationId::KaaJump || id == GameAnimationId::KaaFall;
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
        const bool looping = animation.looping && !holdsLastFrame(currentAnimation);

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
                if (looping) {
                    currentFrameIndex = 0;
                } else {
                    currentFrameIndex = animation.frames.size() - 1;
                }
            }

            applyCurrentAnimationFrame(scene);

            if (!looping && currentFrameIndex == animation.frames.size() - 1) {
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