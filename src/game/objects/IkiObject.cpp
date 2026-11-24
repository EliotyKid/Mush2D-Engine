#include "game/objects/IkiObject.hpp"

#include "engine/world/Collision.hpp"
#include "engine/world/Effects.hpp"
#include "engine/world/Resources.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/SpriteAnimator.hpp"
#include "game/iki/states/DeadState.hpp"
#include "game/iki/states/FallState.hpp"
#include "game/iki/states/HurtState.hpp"
#include "game/iki/states/IdleState.hpp"
#include "game/iki/states/JumpState.hpp"
#include "game/iki/states/RunState.hpp"
#include "game/objects/SlimeEnemy.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace {
constexpr float TurnSpeed = 2.0f; // emitMovementCues' default turnThreshold

float moveTowards(float value, float target, float maxStep) {
    return value < target ? std::min(value + maxStep, target) : std::max(value - maxStep, target);
}
} // namespace

void IkiObject::onCreate(Scene2D&) {
    name = "Iki";
    addTag("player");
    sprite = SpriteData{.layer = 2};
    collider = ColliderData{.size = {0.6f, 1.6f}, .offset = {0.0f, 0.8f}}; // position = feet
    addComponent<Resources>(); // the prefab's "components" entry tunes this one

    machine_.add(std::make_unique<IdleState>());
    machine_.add(std::make_unique<RunState>());
    machine_.add(std::make_unique<JumpState>());
    machine_.add(std::make_unique<FallState>());
    machine_.add(std::make_unique<HurtState>());
    machine_.add(std::make_unique<DeadState>());
    // The state name is the animation key (prefab "animations").
    machine_.onChange = [this](const char*, const char* to) {
        if (auto* animator = getComponent<SpriteAnimator>()) animator->setState(to);
    };
    registerStateMachine("move", &machine_);
    machine_.start(*this, "idle");
    checkpoint_ = transform.position;
}

void IkiObject::onPrefabApplied(Scene2D&) {
    // The prefab added the animator after start(): give it the current state.
    if (auto* animator = getComponent<SpriteAnimator>()) animator->setState(stateName());
    checkpoint_ = transform.position;
}

void IkiObject::applyInput(const InputState& input) { input_ = input; }

void IkiObject::onStep(Scene2D& scene, float dt) {
    // Fixed order (spec decision 2): input (applyInput, before the step), JumpAssist, states. The components
    // then step in the prefab's order: SpriteAnimator, SquashStretch, VelocityLean, Blink. The cues below
    // are events: their effects play at the next flush.
    scene_ = &scene;
    invulnerableLeft_ = std::max(0.0f, invulnerableLeft_ - dt);
    jump.coyoteTime = coyoteTime;
    jump.jumpBuffer = jumpBuffer;
    jump.tick(dt, grounded, input_.jump.pressed);
    const MoveResult previous = lastMove_;
    machine_.update(*this, dt);
    flipX = facing < 0.0f;
    if (!placed_) emitMovementCues(scene, *this, previous, lastMove_, stepVelocity_, maxFallSpeed);
    placed_ = false;
}

void IkiObject::applyGravity(float dt) {
    float g = gravity;
    if (!grounded && std::abs(velocity_.y) < apexThreshold && jumpHeld()) g *= apexGravityFactor;
    else if (velocity_.y < 0.0f) g *= fallGravityFactor;
    velocity_.y = std::max(velocity_.y + g * dt, maxFallSpeed);
}

void IkiObject::runHorizontal(float dt, float inputX) {
    // Reversing on the ground at speed: "turned" (skid dust). emitMovementCues only sees a turn when the
    // velocity flips sign at speed, which the deceleration never allows.
    if (grounded && inputX * velocity_.x < 0.0f && std::abs(velocity_.x) >= TurnSpeed && inputX * facing < 0.0f && scene_)
        emitMovementCue(*scene_, *this, "turned");
    if (inputX != 0.0f) facing = inputX > 0.0f ? 1.0f : -1.0f;
    const float target = inputX * runSpeed;
    // Speeding up toward the input uses the acceleration; slowing down or turning uses the deceleration.
    const bool speedingUp = target != 0.0f && target * velocity_.x >= 0.0f && std::abs(target) >= std::abs(velocity_.x);
    const float rate = grounded ? (speedingUp ? groundAccel : groundDecel) : (speedingUp ? airAccel : airDecel);
    velocity_.x = nextVelocityX(lastMove_, moveTowards(velocity_.x, target, rate * dt));
}

bool IkiObject::tryJump() {
    if (!jump.consumeJump()) return false;
    if (downHeld() && requestDropThrough(body_, lastMove_)) return false; // down + Z on a one-way: drop through
    velocity_.y = jumpSpeed;
    jumped_ = true;
    jumpCuttable = true;
    if (scene_) emitMovementCue(*scene_, *this, "jumped");
    return true;
}

void IkiObject::moveAndCollide(float dt) {
    if (!scene_ || !collider) return;
    Scene2D& scene = *scene_;
    body_.position = transform.position + collider->offset;
    body_.halfSize = collider->size * 0.5f;
    stepVelocity_ = velocity_;

    const float reach = glm::length(velocity_) * dt + 1.0f + cornerCorrection;
    const Collision2D::AABB area{body_.position - body_.halfSize - reach, body_.position + body_.halfSize + reach};
    const CollisionQuery query{&scene.collision, Collision2D::solidBoxes(scene, id, area)};

    // Corner correction: rising into the corner of a ceiling, slide sideways up to cornerCorrection when that
    // frees the way up (the nearest free shift wins, left first on a tie).
    if (velocity_.y > 0.0f && cornerCorrection > 0.0f) {
        auto bumps = [&](glm::vec2 at) {
            Body probe = body_;
            probe.position = at;
            return move(probe, {0.0f, velocity_.y}, dt, query, false, true).hitCeiling;
        };
        if (bumps(body_.position)) {
            constexpr float Step = 0.025f;
            bool done = false;
            for (float d = Step; d <= cornerCorrection + 1e-4f && !done; d += Step) {
                for (float side : {-1.0f, 1.0f}) {
                    Body probe = body_;
                    const MoveResult r = move(probe, {side * d / dt, 0.0f}, dt, query, false, true);
                    if (std::abs(r.position.x - body_.position.x - side * d) > 1e-4f || bumps(r.position)) continue;
                    body_.position = r.position;
                    done = true;
                    break;
                }
            }
        }
    }

    lastMove_ = move(body_, velocity_, dt, query, grounded, jumped_);
    jumped_ = false;
    transform.position = lastMove_.position - collider->offset;
    velocity_ = lastMove_.velocity;
    grounded = lastMove_.grounded;
    if (grounded && stompBelow(scene)) grounded = false; // the bounce leaves the ground
}

const char* IkiObject::nextLocomotion(const char* current) const {
    const char* next = grounded ? (moveX() != 0.0f ? "run" : "idle") : (velocity_.y > 0.0f ? "jump" : "fall");
    return std::strcmp(next, current) == 0 ? nullptr : next;
}

// Landed on a slime whose centre is under the feet: it dies and the Iki bounces (same rule and SlimeEnemy
// API as PlayerObject::stompBelow).
bool IkiObject::stompBelow(Scene2D& scene) {
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
    if (stomped) {
        velocity_.y = jumpSpeed * 0.6f;
        jumped_ = true;
        jumpCuttable = false;
    }
    return stomped;
}

void IkiObject::hurt(const DamageInfo& info) {
    auto* resources = getComponent<Resources>();
    if (!resources || invulnerable() || resources->health() == 0 || info.amount <= 0) return;
    resources->damage(info.amount); // health 0: Resources emits Died (the game runs the death)
    invulnerableLeft_ = invulnerableTime;
    if (resources->health() == 0) {
        machine_.change(*this, "dead");
        return;
    }
    if (info.direction.x != 0.0f) velocity_.x = (info.direction.x > 0.0f ? 1.0f : -1.0f) * hurtKnockback.x;
    if (info.direction != glm::vec2(0.0f)) {
        velocity_.y = hurtKnockback.y;
        jumped_ = true;
    }
    jumpCuttable = false;
    machine_.change(*this, "hurt");
}

void IkiObject::respawnAt(glm::vec2 position) {
    transform.position = position;
    velocity_ = glm::vec2(0.0f);
    lastMove_ = {};
    grounded = false;
    jumped_ = false;
    placed_ = true;
    body_.droppingThrough = nullptr;
}

void IkiObject::respawn() {
    if (auto* resources = getComponent<Resources>()) resources->refill();
    invulnerableLeft_ = 0.0f;
    velocity_ = glm::vec2(0.0f);
    jump = JumpAssist{};
    jumpCuttable = false;
    machine_.change(*this, "idle");
}
