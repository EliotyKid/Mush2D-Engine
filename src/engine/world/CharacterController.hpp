#pragma once

#include "CollisionTypes.hpp"
#include "CollisionWorld.hpp"

#include <vector>

#include <glm/glm.hpp>

// Platformer body: an AABB (centre + half size) whose bottom corners are rounded (radius 0.1 u)
// when it collides with segments.
struct Body {
    glm::vec2 position{0.0f}, halfSize{0.3f, 0.8f};
    float dropThroughTimer = 0.0f;
    const Segment* droppingThrough = nullptr; // only compared with live segments, never dereferenced
};

struct MoveResult {
    glm::vec2 position{0.0f}, velocity{0.0f};
    bool grounded = false, onOneWay = false, hitCeiling = false, onSteep = false;
    glm::vec2 groundNormal{0.0f}; // walkable ground or steep surface touched; zero in the air
    const Segment* groundSegment = nullptr; // the one-way segment stood on (onOneWay only)
};

struct CollisionQuery {
    const CollisionWorld* world = nullptr;
    std::vector<Collision2D::AABB> solids; // dynamic solid boxes, without the moving body itself
};

// Moves the body by velocity * dt (velocity already includes input, jump and gravity) and resolves
// collisions. Ground is a surface with normal.y >= cos(50 deg). While wasGrounded && !jumpedThisFrame
// the body walks along the ground tangent at the flat horizontal speed and gravity is dropped; at the
// end it snaps down to ground up to 0.3 u below. One-way segments collide only from above.
MoveResult move(Body& body, glm::vec2 velocity, float dt, const CollisionQuery& query, bool wasGrounded,
                bool jumpedThisFrame);
// The caller's horizontal velocity for the next frame: input, except while sliding on a steep surface,
// where the slide keeps last frame's v.x (frame-rate independent) unless input points away from it.
inline float nextVelocityX(const MoveResult& last, float inputVx) {
    return last.onSteep && inputVx * last.groundNormal.x <= 0.0f ? last.velocity.x : inputVx;
}
// Down + jump: ignore one-way segments (all of them: a branch is a chain of segments) for 0.25 s or until
// the body's base passes below the one stood on. False (do a normal jump instead) unless the body stands
// only on one-way ground.
bool requestDropThrough(Body& body, const MoveResult& last);

class GameObject;
class Scene2D;
// Juice cues for any object moved by move() (MovementCue, Effects.hpp), called once per step with the last
// two results and this step's velocity: "landed" (strength = |velocity.y| / |maxFallSpeed|, limited to 1),
// "left_ground", and "turned" (on the ground, reversed from a speed of at least turnThreshold).
void emitMovementCues(Scene2D& scene, const GameObject& obj, const MoveResult& prev, const MoveResult& now,
                      glm::vec2 velocity, float maxFallSpeed, float turnThreshold = 2.0f);
// Hot reload (spec decision 8): a body that overlaps the reloaded geometry moves to the nearest free spot at
// most maxPush away (straight up first); a free body stays. False (body unchanged) when there is none.
bool depenetrate(Body& body, const CollisionQuery& query, float maxPush = 2.0f);

struct SurfaceHit { bool hit = false; Surface surface = Surface::Default; };
// A wall (not ground, not ceiling) facing the body within `reach` of its side (-1 left, +1 right), or a
// ceiling (normal.y < -0.5) within `reach` above it. Works while the body stands still beside it. IceClimb
// wins when both surfaces touch; dynamic solids count as Default.
SurfaceHit probeWall(const Body& body, int side, const CollisionQuery& query, float reach = 0.05f);
SurfaceHit probeCeiling(const Body& body, const CollisionQuery& query, float reach = 0.05f);
