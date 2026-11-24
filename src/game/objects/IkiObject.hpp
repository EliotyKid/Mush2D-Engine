#pragma once

#include "engine/platform/InputState.hpp"
#include "engine/world/CharacterController.hpp"
#include "engine/world/Damage.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/IPlayer.hpp"
#include "engine/world/JumpAssist.hpp"
#include "engine/world/StateMachine.hpp"

#include <glm/glm.hpp>

class Scene2D;

// Ikigai's playable character (spec D2 "IkiObject"); prefab assets/prefabs/iki.prefab.json. Tagged "player".
// transform.position is the feet (the frame pivot); the collider sits above it. Movement lives in the states
// of the "move" machine (src/game/iki/states/), which call the physics helpers below. The Iki has no effect
// code: it emits MovementCue "jumped" and calls emitMovementCues every step; the prefab's "effects" section,
// SquashStretch, VelocityLean and Blink do the juice. Its Resources component (added here, tuned by the
// prefab) regenerates by itself.
class IkiObject : public GameObject, public IPlayer {
    MUSH2D_OBJECT(IkiObject, GameObject)
public:
    // Run (units / s and units / s^2).
    MUSH2D_EXPORT(float, runSpeed, 5.0f)
    MUSH2D_EXPORT(float, groundAccel, 60.0f)
    MUSH2D_EXPORT(float, groundDecel, 70.0f)
    MUSH2D_EXPORT(float, airAccel, 35.0f)
    MUSH2D_EXPORT(float, airDecel, 20.0f)
    // Jump. Gravity is multiplied by apexGravityFactor near the top while Z is held (|vy| < apexThreshold)
    // and by fallGravityFactor going down. Releasing Z while rising multiplies vy by jumpCutFactor, once.
    MUSH2D_EXPORT(float, gravity, -32.0f)
    MUSH2D_EXPORT(float, jumpSpeed, 11.0f)
    MUSH2D_EXPORT(float, jumpCutFactor, 0.5f)
    MUSH2D_EXPORT(float, apexThreshold, 1.5f)
    MUSH2D_EXPORT(float, apexGravityFactor, 0.5f)
    MUSH2D_EXPORT(float, fallGravityFactor, 1.6f)
    MUSH2D_EXPORT(float, maxFallSpeed, -18.0f)
    MUSH2D_EXPORT(float, coyoteTime, 0.1f)
    MUSH2D_EXPORT(float, jumpBuffer, 0.12f)
    MUSH2D_EXPORT(float, cornerCorrection, 0.2f) // rising into a ceiling corner: side shift allowed (units)
    // Ice stakes (used by the wall and ceiling states, D2 Task 6).
    MUSH2D_EXPORT(float, wallClimbSpeed, 3.0f)
    MUSH2D_EXPORT(glm::vec2, wallJumpVelocity, glm::vec2(6.0f, 10.0f)) // x away from the wall, y up
    MUSH2D_EXPORT(float, wallJumpLock, 0.15f)
    MUSH2D_EXPORT(glm::vec2, ledgeHop, glm::vec2(2.0f, 7.0f))
    MUSH2D_EXPORT(float, ceilingMoveSpeed, 3.0f)
    // Damage. hurtKnockback: x along the sign of the hit's direction.x, y along direction.y (up when it is 0).
    MUSH2D_EXPORT(float, hurtDuration, 0.35f)
    MUSH2D_EXPORT(glm::vec2, hurtKnockback, glm::vec2(6.0f, 6.0f))
    MUSH2D_EXPORT(float, invulnerableTime, 0.8f)

    // Physics state the states read and write.
    glm::vec2 velocity_{0.0f};
    float facing = 1.0f; // +1 right, -1 left
    bool grounded = false;
    JumpAssist jump;
    bool jumpCuttable = false; // set by tryJump: only a Z jump is cut on release (not a stomp or a knockback)

    void onCreate(Scene2D& scene) override;
    void onPrefabApplied(Scene2D& scene) override;
    void onStep(Scene2D& scene, float dt) override;

    // IPlayer
    void applyInput(const InputState& input) override;
    void respawnAt(glm::vec2 position) override; // teleport only: no velocity, ground or slide carried over
    bool downPressedThisFrame() const override { return input_.moveDown.pressed; }
    void setCheckpoint(glm::vec2 position) override { checkpoint_ = position; }
    void respawnAtCheckpoint() override { respawnAt(checkpoint_); respawn(); }

    // Ignored while dead or invulnerable. Health 0 -> "dead" (Resources emits Died); else "hurt", pushed along
    // info.direction.
    void hurt(const DamageInfo& info);
    // After death: full Resources, no invulnerability, velocity or jump timers, state "idle".
    void respawn();
    const char* stateName() const { return machine_.current(); }
    StateMachine<IkiObject>& states() { return machine_; }
    bool invulnerable() const { return invulnerableLeft_ > 0.0f; }

    // Input as the states see it (Hurt and Dead ignore it).
    float moveX() const { return float(input_.moveRight.down) - float(input_.moveLeft.down); }
    bool jumpHeld() const { return input_.jump.down; }
    bool downHeld() const { return input_.moveDown.down; }

    // Physics shared by the states, in this order each step: runHorizontal, tryJump, applyGravity, moveAndCollide.
    void applyGravity(float dt);
    void runHorizontal(float dt, float inputX); // eases velocity_.x to inputX * runSpeed; faces the input
    bool tryJump();                              // buffered Z on the ground or in coyote time (down + Z drops through)
    void moveAndCollide(float dt);               // controller move, corner correction, slime stomp
    // The locomotion state the body is in now (idle, run, jump or fall); nullptr when it is `current`.
    const char* nextLocomotion(const char* current) const;

private:
    StateMachine<IkiObject> machine_;
    Scene2D* scene_ = nullptr; // the scene of the running step (physics helpers need it)
    InputState input_{};
    Body body_;
    MoveResult lastMove_;
    glm::vec2 stepVelocity_{0.0f}; // velocity before this step's move (landing strength, turn)
    glm::vec2 checkpoint_{0.0f};
    float invulnerableLeft_ = 0.0f;
    bool jumped_ = false; // left the ground this step (the controller must not snap it back down)
    bool placed_ = true;  // just created or teleported: no movement cues for the first step (no landing puff)

    bool stompBelow(Scene2D& scene);
};
