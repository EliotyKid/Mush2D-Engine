#pragma once

#include "engine/platform/InputState.hpp"
#include "engine/world/CharacterController.hpp"
#include "engine/world/Combat.hpp"
#include "engine/world/Damage.hpp"
#include "engine/world/EventBus.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/IPlayer.hpp"
#include "engine/world/JumpAssist.hpp"
#include "engine/world/StateMachine.hpp"
#include "iki_demo/abilities/IceStakesAbility.hpp" // here so all_objects.cpp registers the component

#include <glm/glm.hpp>

class Scene2D;

// Ikigai's playable character (spec D2 "IkiObject"); prefab assets/prefabs/iki.prefab.json. Tagged "player".
// transform.position is the feet (the frame pivot); the collider sits above it. Movement lives in the states
// of the "move" machine (examples/iki_demo/src/iki_demo/iki/states/), which call the physics helpers below. The Iki has no effect
// code: it emits MovementCue "jumped", "hurt" (strength = invulnerableTime) and calls emitMovementCues every
// step; the prefab's "effects" section, SquashStretch, VelocityLean and Blink do the juice. Its Resources
// component (added here, tuned by the prefab) regenerates by itself.
// Combat (spec D3) runs on a second machine, "action" (examples/iki_demo/src/iki_demo/iki/actions/): none, slash1..3, shoot, guard,
// guard_break. Damage arrives through the Damageable (added here with a Hurtbox): its guard callback is the
// shield, and each DamageTaken calls hurt (the reaction). The prefab's children Slash1..3 hold the Hitboxes.
// Cues: "hit" / "finisher" when its hit deals damage (slash 3 = finisher), "no_energy", "guard_break".
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
    MUSH2D_EXPORT(float, stompBounceFactor, 0.6f) // a slime stomp bounces at jumpSpeed * this
    MUSH2D_EXPORT(float, turnThreshold, 2.0f)     // u/s: a turn faster than this emits "turned" (turn dust)
    // Ice stakes (states "wall" and "ceiling", with the IceStakesAbility component unlocked).
    MUSH2D_EXPORT(float, wallClimbSpeed, 3.0f)
    MUSH2D_EXPORT(glm::vec2, wallJumpVelocity, glm::vec2(6.0f, 10.0f)) // x away from the wall, y up
    MUSH2D_EXPORT(float, wallJumpLock, 0.15f) // s of ignored horizontal input after a wall jump or a ledge hop
    MUSH2D_EXPORT(glm::vec2, ledgeHop, glm::vec2(2.0f, 7.0f)) // x toward the ledge, y up
    MUSH2D_EXPORT(float, ceilingMoveSpeed, 3.0f)
    MUSH2D_EXPORT(float, climbParticleInterval, 0.1f) // s between "climb" cues while moving on a wall or ceiling
    MUSH2D_EXPORT(float, wallGripHalf, 0.1f)      // half height of the band at the body's middle that grabs a wall
                                                  // (and half width for a ceiling)
    MUSH2D_EXPORT(float, ceilingGrabReach, 0.3f)  // up held this close under an ice ceiling grabs it (u)
    MUSH2D_EXPORT(float, ceilingStickSpeed, 8.0f) // u/s up while hanging: snaps in from the grab reach
    // Damage. hurtKnockback: x along the sign of the hit's direction.x, y along direction.y (up when it is 0).
    MUSH2D_EXPORT(float, hurtDuration, 0.35f)
    MUSH2D_EXPORT(glm::vec2, hurtKnockback, glm::vec2(6.0f, 6.0f))
    MUSH2D_EXPORT(float, invulnerableTime, 0.8f) // copied to the Damageable every step (it runs the timer)
    // Slashes (machine "action"). X within comboWindow after a slash chains the next one, up to 3; X during a
    // slash is kept and chains at its end. Each slash lasts slashDuration and steps attackStep forward on the
    // ground; ground speed is runSpeed * attackMoveFactor meanwhile. Falling in the air, the gravity is multiplied by
    // airAttackGravityFactor and the fall limited to airAttackMaxFall: one sequence per jump. Slash 3's hitbox
    // pushes finisherKnockback (the only source: Slash3's Hitbox knockback in the prefab is left out).
    MUSH2D_EXPORT(float, comboWindow, 0.35f)
    MUSH2D_EXPORT(float, slashDuration, 0.22f)
    MUSH2D_EXPORT(float, attackStep, 0.3f)
    MUSH2D_EXPORT(float, attackMoveFactor, 0.4f)
    MUSH2D_EXPORT(float, airAttackGravityFactor, 0.25f)
    MUSH2D_EXPORT(float, airAttackMaxFall, -3.0f)
    MUSH2D_EXPORT(float, finisherKnockback, 8.0f)
    // Ice shard (C): prefab ice_shard from the ProjectilePool; its speed, range and damage are the prefab's.
    MUSH2D_EXPORT(float, shardCost, 15.0f)
    MUSH2D_EXPORT(float, shootCooldown, 0.25f)
    // Shield (V held, on the ground). A hit from the front costs blockCost ice and pushes blockPush (u/s) back;
    // within parryWindow of raising it, it is a parry (free; the stun is the Damageable's parryStun). Without
    // the ice, the hit passes and the Iki has no shield nor attack for guardBreakStun.
    MUSH2D_EXPORT(float, guardMoveFactor, 0.3f)
    MUSH2D_EXPORT(float, blockCost, 10.0f)
    MUSH2D_EXPORT(float, blockPush, 3.0f)
    MUSH2D_EXPORT(float, parryWindow, 0.15f)
    MUSH2D_EXPORT(float, guardBreakStun, 0.5f)

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

    // The reaction to a DamageTaken (the Damageable already took the health): health 0 -> "dead"; else "hurt",
    // pushed along info.direction. Damage goes through the Damageable: emit a HitEvent at the Iki.
    void hurt(const DamageInfo& info);
    // After death: full Resources, no invulnerability, velocity or jump timers, state "idle".
    void respawn();
    const char* stateName() const { return machine_.current(); }
    StateMachine<IkiObject>& states() { return machine_; }
    bool invulnerable() const;
    StateMachine<IkiObject>& actions() { return actions_; }
    const char* actionName() const { return actions_.current(); }
    bool actionIs(const char* name) const;

    // Input as the states see it (Hurt and Dead ignore it).
    float moveX() const { return float(input_.moveRight.down) - float(input_.moveLeft.down); }
    int moveSide() const { return int(moveX()); } // -1, 0 or +1
    bool jumpHeld() const { return input_.jump.down; }
    bool downHeld() const { return input_.moveDown.down; }
    bool upHeld() const { return input_.moveUp.down; }
    bool attackPressed() const { return input_.attack.pressed; }
    bool shootPressed() const { return input_.shoot.pressed; }
    bool guardPressed() const { return input_.guard.pressed; }
    bool guardHeld() const { return input_.guard.down; }

    // Combat, for the action states.
    bool actionsBlocked() const; // move is wall, ceiling, hurt or dead
    bool canGuard() const;       // move is idle or run
    bool slashing() const;
    // "slash<n>" when slash n may start now (one sequence per jump in the air), else nullptr.
    const char* startSlash(int n);
    void beginSlash(int n);          // SlashAction::enter: step forward, the window source
    void slashWindow(int n, float t); // the hitbox active in [activeStart, activeEnd) unless the animation drives it
    void endSlash(int n);            // the hitbox off
    void openCombo(int next);        // a slash ended: X within comboWindow starts slash `next`
    void resetCombo();
    // "shoot" after firing a shard; nullptr on cooldown or without the ice ("no_energy" cue).
    const char* tryShoot();
    int comboNext = 1;
    float comboLeft = 0.0f;
    bool attackBuffered = false;

    // Physics shared by the states, in this order each step: runHorizontal, tryJump, applyGravity, moveAndCollide.
    void applyGravity(float dt);
    // Eases velocity_.x to inputX * runSpeed; faces the input. Does nothing during wallJumpLock.
    void runHorizontal(float dt, float inputX);
    // Buffered Z on the ground or in coyote time (down + Z drops through); in wall coyote time, a wall jump.
    bool tryJump();
    // Controller move, corner correction (off on a wall or ceiling: it would slide the Iki off), slime stomp.
    void moveAndCollide(float dt, bool cornerCorrect = true);
    // The locomotion state the body is in now (idle, run, jump or fall); nullptr when it is `current`.
    const char* nextLocomotion(const char* current) const;

    // Ice stakes. Grabbing (spec decision 5), called by the air states after moveAndCollide: in the air, holding
    // toward an ice wall -> "wall" (even with up held); else up held under an ice ceiling -> "ceiling". A
    // buffered Z on grabbing a wall wall-jumps at once (returns nullptr: the Iki is rising, "jump").
    const char* nextGrab();
    int wallSide = 1; // the wall being clung to: -1 left, +1 right
    // An ice wall on `side` (stakes unlocked). grip: only the band at the body's middle counts (grabbing: no
    // grab beside a plain wall over ice); else any touch (holding on until the whole body is past the ice).
    bool iceWall(int side, bool grip) const;
    bool anyWall(int side) const; // ice or not
    bool iceCeiling() const;      // under the middle of the body, within the grab reach
    bool canGrabCeiling() const { return upHeld() && !ceilingBlocked_ && iceCeiling(); }
    void wallJump();              // away from wallSide, input locked for wallJumpLock
    void hopLedge();              // the ice ended climbing up with nothing above: onto the ledge
    void letGoWall();             // fall; wall coyote, no re-grab of this wall until its direction is pressed anew
    void letGoCeiling();          // fall; no re-grab of a ceiling until up is pressed anew
    void climbCue(float dt, bool moving, glm::vec2 offset); // "climb" every climbParticleInterval while moving
    void grabCue(glm::vec2 offset);                         // "grab" at the stakes (offset from the feet)
    // Where the stakes bite, from the feet, following the collider: the hands on `side`, the top of the head.
    glm::vec2 handsOffset(int side) const { return {float(side) * 0.5f * collider->size.x, collider->offset.y + 0.25f * collider->size.y}; }
    glm::vec2 headOffset() const { return {0.0f, collider->offset.y + 0.5f * collider->size.y}; }

private:
    StateMachine<IkiObject> machine_;
    StateMachine<IkiObject> actions_;
    std::vector<EventBus::Subscription> subscriptions_;
    GameObjectId slashes_[3]{}; // the children Slash1..3
    bool animWindow_ = false;   // this slash's animation has hit_on: its events drive the hitbox
    bool hitOnSeen_ = false;    // this slash's first hit_on came: later ones (a looping animation) do nothing
    bool airComboUsed_ = false; // a slash started in the air since the last ground, wall, ceiling or damage
    float shootLeft_ = 0.0f;
    Scene2D* scene_ = nullptr; // the scene of the running step (physics helpers need it)
    InputState input_{};
    bool inputRead_ = true; // a step has read input_ (false through a hit stop: its presses are kept)
    Body body_;
    MoveResult lastMove_;
    glm::vec2 stepVelocity_{0.0f}; // velocity before this step's move (landing strength, turn)
    glm::vec2 checkpoint_{0.0f};
    bool jumped_ = false; // left the ground this step (the controller must not snap it back down)
    bool placed_ = true;  // just created or teleported: no movement cues for the first step (no landing puff)
    float wallJumpLockLeft_ = 0.0f;
    float wallCoyoteLeft_ = 0.0f; // after letting go of a wall: Z wall-jumps (JumpAssist runs the same coyote)
    int wallBlockedSide_ = 0;     // let go of this wall: no re-grab until its direction is released
    bool ceilingBlocked_ = false; // let go of a ceiling: no re-grab until up is released
    float climbCueLeft_ = 0.0f;

    bool stompBelow(Scene2D& scene);
    HitResult guard(const DamageInfo& info); // the Damageable's guard callback
    Hitbox* slashHitbox(int n) const;
    bool iceStakes() const;
    Body bodyHere() const;            // the collider box at transform.position
    CollisionQuery query(float reach) const; // static geometry and solids within reach of the body
};
