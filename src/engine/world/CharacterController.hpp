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
// Down + jump: ignore the one-way segment stood on for 0.25 s or until the body's base passes below
// it. False (do a normal jump instead) unless the body stands only on one-way ground.
bool requestDropThrough(Body& body, const MoveResult& last);
