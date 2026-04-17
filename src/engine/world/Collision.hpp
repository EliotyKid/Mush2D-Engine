#pragma once

#include "Scene2D.hpp"

#include <glm/glm.hpp>

namespace Collision2D {

struct AABB {
    glm::vec2 min{0.0f, 0.0f};
    glm::vec2 max{0.0f, 0.0f};
};

bool hasCollider(const GameObject& object);
const ColliderData* getCollider(const GameObject& object);

AABB buildAABB(const GameObject& object);
bool intersects(const AABB& a, const AABB& b);

bool collidesAtPosition(
    const Scene2D& scene,
    GameObjectId movingObjectId,
    const glm::vec2& newPosition
);

}