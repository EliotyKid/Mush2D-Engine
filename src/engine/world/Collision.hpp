#pragma once

#include "Scene2D.hpp"
#include "CollisionTypes.hpp"

#include <vector>

#include <glm/glm.hpp>

namespace Collision2D {

bool hasCollider(const GameObject& object);
const ColliderData* getCollider(const GameObject& object);

AABB buildAABB(const GameObject& object);
bool intersects(const AABB& a, const AABB& b);

// Objects not pending destroy whose grid box overlaps `box`, including inactive ones (active == false);
// callers filter on `active` themselves. The grid is refreshed at the start of each step, so
// an object moved during the current step shows its previous position here.
std::vector<GameObjectId> queryAABB(const Scene2D& scene, const AABB& box, QueryKind kind = QueryKind::Any);

// newPosition is in the mover's own space: the parent's local space if it has a parent, else world.
bool collidesAtPosition(
    const Scene2D& scene,
    GameObjectId movingObjectId,
    const glm::vec2& newPosition
);

}
