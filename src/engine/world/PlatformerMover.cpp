#include "engine/world/PlatformerMover.hpp"

#include "engine/platform/Actions.hpp"
#include "engine/world/Collision.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"

#include <algorithm>
#include <cmath>

void PlatformerMover::onStep(GameObject& owner, Scene2D& scene, float dt) {
    // No input service or no collider: nothing to read or nothing to move.
    const ActionMap* map = scene.service<ActionMap>();
    if (!map || !owner.collider) return;
    // The step frame: a press made while a hit stop froze the scene still counts in this step.
    const ActionFrame& in = map->stepFrame();

    // Jump timing: the buffer window comes from the project's jump action.
    jump_.coyoteTime = coyoteTime;
    jump_.buffer.window = map->bufferWindow(actionJump);
    jump_.tick(dt, last_.grounded, in.pressed(actionJump));

    // Horizontal speed eases toward the wanted speed (no overshoot).
    const float dir = (in.down(actionRight) ? 1.0f : 0.0f) - (in.down(actionLeft) ? 1.0f : 0.0f);
    const float target = dir * runSpeed;
    const float step = acceleration * dt;
    const float toward = velocity_.x + std::clamp(target - velocity_.x, -step, step);
    velocity_.x = nextVelocityX(last_, toward);

    // Jump: drop through a one-way platform when that key is held, otherwise a normal jump.
    body_.position = owner.transform.position + owner.collider->offset;
    body_.halfSize = owner.collider->size * 0.5f;
    bool jumped = false;
    if (jump_.consumeJump() && !(in.down(actionDrop) && requestDropThrough(body_, last_))) {
        velocity_.y = jumpSpeed;
        jumped = true;
        cuttable_ = true;
    }
    // Variable jump height: letting go while still rising cuts the speed, once.
    if (cuttable_ && velocity_.y > 0.0f && !in.down(actionJump)) {
        velocity_.y *= jumpCut;
        cuttable_ = false;
    }
    if (velocity_.y <= 0.0f) cuttable_ = false;
    velocity_.y = std::max(velocity_.y + gravity * dt, maxFallSpeed);

    // Move with collisions: only the solid boxes near the body matter.
    const float reach = glm::length(velocity_) * dt + 1.0f;
    const Collision2D::AABB area{body_.position - body_.halfSize - reach, body_.position + body_.halfSize + reach};
    const CollisionQuery query{&scene.collision, Collision2D::solidBoxes(scene, owner.id, area)};
    last_ = move(body_, velocity_, dt, query, last_.grounded, jumped);
    owner.transform.position = last_.position - owner.collider->offset;
    velocity_ = last_.velocity;
}
