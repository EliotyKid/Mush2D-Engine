#include "Collision.hpp"

namespace Collision2D {

bool hasCollider(const GameObject& object) {
    return object.collider.has_value() && object.collider->enabled;
}

const ColliderData* getCollider(const GameObject& object) {
    if (!hasCollider(object)) {
        return nullptr;
    }

    return &(*object.collider);
}

AABB buildAABB(const GameObject& object) {
    const ColliderData* collider = getCollider(object);
    if (!collider) {
        return {};
    }

    glm::vec2 center = object.transform.position + collider->offset;
    glm::vec2 halfSize = collider->size * 0.5f;

    AABB box{};
    box.min = center - halfSize;
    box.max = center + halfSize;
    return box;
}

bool intersects(const AABB& a, const AABB& b) {
    if (a.max.x <= b.min.x || a.min.x >= b.max.x) {
        return false;
    }

    if (a.max.y <= b.min.y || a.min.y >= b.max.y) {
        return false;
    }

    return true;
}

bool collidesAtPosition(
    const Scene2D& scene,
    GameObjectId movingObjectId,
    const glm::vec2& newPosition
) {
    const GameObject* movingObject = scene.findObject(movingObjectId);
    if (!movingObject || !hasCollider(*movingObject)) {
        return false;
    }

    const ColliderData* movingCollider = getCollider(*movingObject);
    if (!movingCollider) {
        return false;
    }

    glm::vec2 movingCenter = newPosition + movingCollider->offset;
    glm::vec2 movingHalfSize = movingCollider->size * 0.5f;

    AABB movingBox{};
    movingBox.min = movingCenter - movingHalfSize;
    movingBox.max = movingCenter + movingHalfSize;

    for (const auto& otherPtr : scene.getObjects()) {
        if (!otherPtr || !otherPtr->active) {
            continue;
        }

        if (otherPtr->id == movingObjectId) {
            continue;
        }

        if (!hasCollider(*otherPtr)) {
            continue;
        }

        if (otherPtr->collider->isTrigger) {
            continue;
        }

        AABB otherBox = buildAABB(*otherPtr);
        if (intersects(movingBox, otherBox)) {
            return true;
        }
    }

    return false;
}

}