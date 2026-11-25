#include "game/objects/IkiObject.hpp"

#include "engine/world/Collision.hpp"
#include "engine/world/Effects.hpp"
#include "engine/world/Projectile.hpp"
#include "engine/world/Resources.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/SpriteAnimator.hpp"
#include "game/iki/actions/GuardAction.hpp"
#include "game/iki/actions/GuardBreakAction.hpp"
#include "game/iki/actions/NoneAction.hpp"
#include "game/iki/actions/ShootAction.hpp"
#include "game/iki/actions/SlashAction.hpp"
#include "game/iki/states/CeilingHangState.hpp"
#include "game/iki/states/DeadState.hpp"
#include "game/iki/states/FallState.hpp"
#include "game/iki/states/HurtState.hpp"
#include "game/iki/states/IdleState.hpp"
#include "game/iki/states/JumpState.hpp"
#include "game/iki/states/RunState.hpp"
#include "game/iki/states/WallClingState.hpp"
#include "game/objects/SlimeEnemy.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace {

float moveTowards(float value, float target, float maxStep) {
    return value < target ? std::min(value + maxStep, target) : std::max(value - maxStep, target);
}
bool is(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
} // namespace

void IkiObject::onCreate(Scene2D& scene) {
    name = "Iki";
    addTag("player");
    sprite = SpriteData{.layer = 2};
    collider = ColliderData{.size = {0.6f, 1.6f}, .offset = {0.0f, 0.8f}}; // position = feet
    addComponent<Resources>(); // the prefab's "components" entries tune these
    addComponent<Hurtbox>().team = "player"; // size and offset: the prefab's Hurtbox entry

    Damageable& damageable = addComponent<Damageable>();
    damageable.invulnerableTime = invulnerableTime;
    damageable.guard = [this](const DamageInfo& info) { return guard(info); };
    const GameObjectId self = id;
    subscriptions_.push_back(scene.events().subscribe<DamageTaken>([this, self](const DamageTaken& e) {
        if (e.target == self) hurt(e.info);
        // Its own hit dealt damage: a slash 3 weighs more (prefab "move:finisher", else "move:hit").
        if (e.info.source != self || !scene_) return;
        // At the contact point: the centre of the target's hurtbox (else its position).
        const GameObject* target = scene_->findObject(e.target);
        const Hurtbox* box = target ? target->getComponent<Hurtbox>() : nullptr;
        const glm::vec2 at = box ? 0.5f * (box->box().min + box->box().max) : target ? target->world().position : transform.position;
        emitMovementCue(*scene_, *this, actionIs("slash3") && !e.info.projectile ? "finisher" : "hit", 1.0f, at);
    }));
    subscriptions_.push_back(scene.events().subscribe<AnimationEvent>([this, self](const AnimationEvent& e) {
        if (e.object != self || !animWindow_ || !slashing() || (e.name != "hit_on" && e.name != "hit_off")) return;
        if (Hitbox* h = slashHitbox(actionName()[5] - '0')) { // "slash<n>"
            if (e.name == "hit_off") h->deactivate();
            else if (!hitOnSeen_) { // a looping animation repeats hit_on: one window per slash
                hitOnSeen_ = true;
                h->activate();
            }
        }
    }));

    machine_.add(std::make_unique<IdleState>());
    machine_.add(std::make_unique<RunState>());
    machine_.add(std::make_unique<JumpState>());
    machine_.add(std::make_unique<FallState>());
    machine_.add(std::make_unique<HurtState>());
    machine_.add(std::make_unique<DeadState>());
    machine_.add(std::make_unique<WallClingState>());
    machine_.add(std::make_unique<CeilingHangState>());
    // The state name is the animation key (prefab "animations").
    machine_.onChange = [this](const char*, const char* to) {
        if (auto* animator = getComponent<SpriteAnimator>()) animator->setState(to);
    };
    registerStateMachine("move", &machine_);
    machine_.start(*this, "idle");

    actions_.add(std::make_unique<NoneAction>());
    for (int n = 1; n <= 3; ++n) actions_.add(std::make_unique<SlashAction>(n));
    actions_.add(std::make_unique<ShootAction>());
    actions_.add(std::make_unique<GuardAction>());
    actions_.add(std::make_unique<GuardBreakAction>());
    actions_.onChange = [this](const char*, const char* to) {
        if (auto* animator = getComponent<SpriteAnimator>()) animator->action = to; // "action:<to>" animations
    };
    registerStateMachine("action", &actions_);
    actions_.start(*this, "none");
    checkpoint_ = transform.position;
}

void IkiObject::onPrefabApplied(Scene2D& scene) {
    // The prefab added the animator after start(): give it the current states.
    if (auto* animator = getComponent<SpriteAnimator>()) {
        animator->setState(stateName());
        animator->action = actionName();
    }
    for (GameObjectId child : children()) // the slash hitboxes: children Slash1..3
        if (const GameObject* o = scene.findObject(child))
            for (int n = 1; n <= 3; ++n)
                if (o->name == "Slash" + std::to_string(n)) slashes_[n - 1] = child;
    checkpoint_ = transform.position;
}

void IkiObject::applyInput(const InputState& input) {
    InputState next = input;
    // The last step was frozen by a hit stop and did not read the input: its presses still count. A pause or an
    // inactive Iki (a step without the Iki) keeps nothing.
    if (!inputRead_ && scene_ && scene_->lastStepFrozen()) keepEdges(next, input_);
    input_ = next;
    inputRead_ = false;
}

void IkiObject::onStep(Scene2D& scene, float dt) {
    // Fixed order (spec decision 2): input (applyInput, before the step), JumpAssist, states. The components
    // then step in the prefab's order: SpriteAnimator, SquashStretch, VelocityLean, Blink. The cues below
    // are events: their effects play at the next flush.
    scene_ = &scene;
    inputRead_ = true;
    if (auto* damageable = getComponent<Damageable>()) damageable->invulnerableTime = invulnerableTime;
    comboLeft = std::max(0.0f, comboLeft - dt);
    shootLeft_ = std::max(0.0f, shootLeft_ - dt);
    if (grounded) airComboUsed_ = false;
    jump.coyoteTime = coyoteTime;
    jump.jumpBuffer = jumpBuffer;
    jump.tick(dt, grounded, input_.jump.pressed);
    wallJumpLockLeft_ = grounded ? 0.0f : std::max(0.0f, wallJumpLockLeft_ - dt);
    wallCoyoteLeft_ = grounded ? 0.0f : std::max(0.0f, wallCoyoteLeft_ - dt);
    if (grounded || moveX() * float(wallBlockedSide_) <= 0.0f) wallBlockedSide_ = 0;
    if (grounded || !upHeld()) ceilingBlocked_ = false;
    const MoveResult previous = lastMove_;
    const int previousFacing = int(facing);
    actions_.update(*this, dt); // first: the move states read this step's action (speed factor, air gravity)
    machine_.update(*this, dt);
    if (actionsBlocked()) { // wall, ceiling, hurt, dead: no action (a guard break outlasts its hurt)
        resetCombo();
        airComboUsed_ = false;
        if (!actionIs("none") && !(actionIs("guard_break") && is(stateName(), "hurt"))) actions_.change(*this, "none");
    }
    flipX = facing < 0.0f;
    // The facing tells "turned" apart from braking: the Iki slows down before its velocity flips.
    if (!placed_) emitMovementCues(scene, *this, previous, lastMove_, stepVelocity_, maxFallSpeed, turnThreshold, previousFacing, int(facing));
    placed_ = false;
}

void IkiObject::applyGravity(float dt) {
    float g = gravity;
    if (!grounded && slashing() && velocity_.y <= 0.0f) { // an air slash hangs once it falls (R7)
        velocity_.y = std::max(velocity_.y + g * airAttackGravityFactor * dt, airAttackMaxFall);
        return;
    }
    if (!grounded && std::abs(velocity_.y) < apexThreshold && jumpHeld()) g *= apexGravityFactor;
    else if (velocity_.y < 0.0f) g *= fallGravityFactor;
    velocity_.y = std::max(velocity_.y + g * dt, maxFallSpeed);
}

void IkiObject::runHorizontal(float dt, float inputX) {
    if (wallJumpLockLeft_ > 0.0f) return; // a wall jump or a ledge hop keeps its push: no steering back to the wall
    if (inputX != 0.0f) facing = inputX > 0.0f ? 1.0f : -1.0f;
    const float factor = actionIs("guard") ? guardMoveFactor : slashing() && grounded ? attackMoveFactor : 1.0f;
    const float target = inputX * runSpeed * factor;
    // Speeding up toward the input uses the acceleration; slowing down or turning uses the deceleration.
    const bool speedingUp = target != 0.0f && target * velocity_.x >= 0.0f && std::abs(target) >= std::abs(velocity_.x);
    const float rate = grounded ? (speedingUp ? groundAccel : groundDecel) : (speedingUp ? airAccel : airDecel);
    velocity_.x = nextVelocityX(lastMove_, moveTowards(velocity_.x, target, rate * dt));
}

bool IkiObject::tryJump() {
    if (actionIs("guard") || !jump.consumeJump()) return false; // no jump behind the shield
    if (wallCoyoteLeft_ > 0.0f) {
        wallJump();
        return true;
    }
    if (downHeld() && requestDropThrough(body_, lastMove_)) return false; // down + Z on a one-way: drop through
    velocity_.y = jumpSpeed;
    jumped_ = true;
    jumpCuttable = true;
    if (scene_) emitMovementCue(*scene_, *this, "jumped");
    return true;
}

Body IkiObject::bodyHere() const {
    Body b = body_;
    if (collider) {
        b.position = transform.position + collider->offset;
        b.halfSize = collider->size * 0.5f;
    }
    return b;
}

CollisionQuery IkiObject::query(float reach) const {
    if (!scene_) return {};
    const Body b = bodyHere();
    const Collision2D::AABB area{b.position - b.halfSize - reach, b.position + b.halfSize + reach};
    return {&scene_->collision, Collision2D::solidBoxes(*scene_, id, area)};
}

void IkiObject::moveAndCollide(float dt, bool cornerCorrect) {
    if (!scene_ || !collider) return;
    Scene2D& scene = *scene_;
    body_ = bodyHere();
    stepVelocity_ = velocity_;
    const CollisionQuery query = this->query(glm::length(velocity_) * dt + 1.0f + cornerCorrection);

    // Corner correction: rising into the corner of a ceiling, slide sideways up to cornerCorrection when that
    // frees the way up (the nearest free shift wins, left first on a tie).
    if (cornerCorrect && velocity_.y > 0.0f && cornerCorrection > 0.0f) {
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
        velocity_.y = jumpSpeed * stompBounceFactor;
        jumped_ = true;
        jumpCuttable = false;
    }
    return stomped;
}

void IkiObject::hurt(const DamageInfo& info) {
    auto* resources = getComponent<Resources>();
    if (!resources || is(stateName(), "dead")) return; // health 0: Resources emitted Died (the game runs the death)
    if (scene_) emitMovementCue(*scene_, *this, "hurt", invulnerableTime); // the prefab blinks this long
    wallJumpLockLeft_ = wallCoyoteLeft_ = 0.0f; // the hit ends a wall or ceiling grab: no wall jump from it
    jump.clearBuffer();
    if (resources->health() == 0) {
        machine_.change(*this, "dead");
        return;
    }
    if (info.direction.x != 0.0f) velocity_.x = (info.direction.x > 0.0f ? 1.0f : -1.0f) * hurtKnockback.x;
    // Up or down along direction.y; a sideways hit pops the Iki up.
    if (info.direction.y != 0.0f) velocity_.y = (info.direction.y > 0.0f ? 1.0f : -1.0f) * hurtKnockback.y;
    else if (info.direction.x != 0.0f) velocity_.y = hurtKnockback.y;
    if (velocity_.y > 0.0f) jumped_ = true; // leaves the ground
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
    if (auto* damageable = getComponent<Damageable>()) damageable->clearInvulnerability();
    resetCombo();
    airComboUsed_ = false;
    shootLeft_ = 0.0f;
    actions_.change(*this, "none");
    velocity_ = glm::vec2(0.0f);
    jump = JumpAssist{};
    jumpCuttable = false;
    wallJumpLockLeft_ = wallCoyoteLeft_ = 0.0f;
    wallBlockedSide_ = 0;
    ceilingBlocked_ = false;
    machine_.change(*this, "idle");
}

// --- Ice stakes ---

bool IkiObject::iceStakes() const {
    const auto* ability = getComponent<IceStakesAbility>();
    return ability && scene_ && ability->unlocked(*scene_);
}

bool IkiObject::iceWall(int side, bool grip) const {
    if (!iceStakes()) return false;
    Body b = bodyHere();
    if (grip) b.halfSize.y = wallGripHalf;
    return probeWall(b, side, query(1.0f)).surface == Surface::IceClimb;
}

bool IkiObject::anyWall(int side) const { return probeWall(bodyHere(), side, query(1.0f)).hit; }

bool IkiObject::iceCeiling() const {
    if (!iceStakes()) return false;
    Body b = bodyHere();
    b.halfSize.x = wallGripHalf;
    return probeCeiling(b, query(1.0f), ceilingGrabReach).surface == Surface::IceClimb;
}

const char* IkiObject::nextGrab() {
    if (grounded) return nullptr;
    const int side = moveSide();
    const bool justLeft = wallJumpLockLeft_ > 0.0f && side == wallSide; // the wall just jumped or hopped from
    if (side != 0 && side != wallBlockedSide_ && !justLeft && iceWall(side, true)) {
        wallSide = side;
        if (!jump.hasBufferedJump()) return "wall";
        wallJump();
        return nullptr;
    }
    if (canGrabCeiling()) return "ceiling";
    return nullptr;
}

void IkiObject::wallJump() {
    jump.clearBuffer();
    wallCoyoteLeft_ = 0.0f;
    velocity_ = {-float(wallSide) * wallJumpVelocity.x, wallJumpVelocity.y};
    facing = -float(wallSide);
    wallJumpLockLeft_ = wallJumpLock;
    jumped_ = true;
    jumpCuttable = true;
    if (scene_) emitMovementCue(*scene_, *this, "jumped");
}

void IkiObject::hopLedge() {
    velocity_ = {float(wallSide) * ledgeHop.x, ledgeHop.y};
    facing = float(wallSide);
    wallJumpLockLeft_ = wallJumpLock; // carries the Iki over the edge
    jumped_ = true;
    jumpCuttable = false;
}

void IkiObject::letGoWall() {
    wallBlockedSide_ = wallSide;
    wallCoyoteLeft_ = coyoteTime;
    jump.startCoyote();
    velocity_ = {0.0f, std::min(velocity_.y, 0.0f)};
}

void IkiObject::letGoCeiling() {
    ceilingBlocked_ = true;
    jump.clearBuffer(); // Z lets go: it must not jump on landing
    velocity_.y = 0.0f;
}

void IkiObject::climbCue(float dt, bool moving, glm::vec2 offset) {
    if (!moving) {
        climbCueLeft_ = 0.0f; // the first move sheds shards at once
        return;
    }
    climbCueLeft_ -= dt;
    if (climbCueLeft_ > 0.0f || !scene_) return;
    climbCueLeft_ = climbParticleInterval;
    emitMovementCue(*scene_, *this, "climb", 1.0f, transform.position + offset);
}

void IkiObject::grabCue(glm::vec2 offset) {
    if (scene_) emitMovementCue(*scene_, *this, "grab", 1.0f, transform.position + offset);
}

// --- Combat (machine "action") ---

bool IkiObject::invulnerable() const {
    const auto* damageable = getComponent<Damageable>();
    return damageable && damageable->invulnerable();
}

bool IkiObject::actionIs(const char* name) const { return is(actions_.current(), name); }

bool IkiObject::actionsBlocked() const {
    const char* m = machine_.current();
    return is(m, "wall") || is(m, "ceiling") || is(m, "hurt") || is(m, "dead");
}

bool IkiObject::canGuard() const { return is(stateName(), "idle") || is(stateName(), "run"); }

bool IkiObject::slashing() const { return std::strncmp(actions_.current(), "slash", 5) == 0; }

Hitbox* IkiObject::slashHitbox(int n) const {
    const GameObject* o = scene_ && n >= 1 && n <= 3 ? scene_->findObject(slashes_[n - 1]) : nullptr;
    return o ? o->getComponent<Hitbox>() : nullptr;
}

const char* IkiObject::startSlash(int n) {
    if (!grounded && n == 1 && airComboUsed_) return nullptr; // one sequence per jump
    return n == 1 ? "slash1" : n == 2 ? "slash2" : "slash3";
}

void IkiObject::beginSlash(int n) {
    attackBuffered = false;
    comboLeft = 0.0f;
    if (!grounded) airComboUsed_ = true; // a slash that starts in the air uses the jump's sequence
    if (Hitbox* h = slashHitbox(n); h && n == 3) h->knockback = finisherKnockback;
    // The animation drives the hitbox when it has a hit_on event (prefab "animationEvents").
    animWindow_ = false;
    hitOnSeen_ = false;
    if (const auto* animator = getComponent<SpriteAnimator>()) {
        const auto anim = animator->animations.find(std::string("action:") + actionName());
        const auto events = anim == animator->animations.end() ? animator->animationEvents.end() : animator->animationEvents.find(anim->second);
        if (events != animator->animationEvents.end())
            for (const auto& [frame, event] : events->second) animWindow_ = animWindow_ || event == "hit_on";
    }
    if (!grounded || !scene_ || !collider || attackStep <= 0.0f) return;
    // The step forward goes through the controller: a wall stops it.
    body_ = bodyHere();
    const MoveResult r = move(body_, {facing * attackStep, 0.0f}, 1.0f, query(attackStep + 1.0f), true, false);
    transform.position = r.position - collider->offset;
}

void IkiObject::slashWindow(int n, float t) {
    Hitbox* h = slashHitbox(n);
    if (!h || animWindow_) return;
    const bool on = t >= h->activeStart && t < h->activeEnd;
    if (on && !h->active()) h->activate();
    else if (!on && h->active()) h->deactivate();
}

void IkiObject::endSlash(int n) {
    if (Hitbox* h = slashHitbox(n)) h->deactivate();
}

void IkiObject::openCombo(int next) {
    comboNext = next;
    comboLeft = comboWindow;
}

void IkiObject::resetCombo() {
    comboNext = 1;
    comboLeft = 0.0f;
    attackBuffered = false;
}

const char* IkiObject::tryShoot() {
    auto* pool = scene_ ? scene_->service<ProjectilePool>() : nullptr;
    auto* resources = getComponent<Resources>();
    if (!pool || !resources || shootLeft_ > 0.0f) return nullptr;
    if (resources->energy(Element::Ice) < shardCost) {
        emitMovementCue(*scene_, *this, "no_energy");
        return nullptr;
    }
    // From the body centre: never inside a wall the Iki touches (its own hurtbox is on its team).
    const glm::vec2 at = transform.position + glm::vec2(0.0f, collider ? collider->offset.y : 0.8f);
    if (!pool->spawn("ice_shard", at, {facing, 0.0f}, "player", id)) return nullptr; // the pool warned: no cost
    resources->spend(Element::Ice, shardCost);
    shootLeft_ = shootCooldown;
    resetCombo();
    return "shoot";
}

// Spec D3 "Escudo e aparo" and decisions 3 and 7. direction pushes the Iki: from the front it points against the
// facing. Only the sign of x counts; a vertical hit (x about 0) counts as from the front.
HitResult IkiObject::guard(const DamageInfo& info) {
    if (!actionIs("guard")) return HitResult::Hit;
    const float x = std::abs(info.direction.x) < 1e-4f ? 0.0f : info.direction.x;
    if (x * facing > 0.0f) return HitResult::Hit; // from behind
    if (info.parryable && actions_.timeInState() < parryWindow) return HitResult::Parried; // scene time: a hit stop holds it
    auto* resources = getComponent<Resources>();
    if (resources && resources->spend(Element::Ice, blockCost)) {
        velocity_.x = -facing * blockPush;
        return HitResult::Blocked;
    }
    actions_.change(*this, "guard_break");
    if (scene_) emitMovementCue(*scene_, *this, "guard_break");
    return HitResult::Hit;
}
