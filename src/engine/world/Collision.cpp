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

    glm::vec2 center = object.world().position + collider->offset;
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

std::vector<GameObjectId> queryAABB(const Scene2D& scene, const AABB& box, QueryKind kind) {
    std::vector<GameObjectId> ids;
    scene.grid().query(box, ids, kind);
    std::erase_if(ids, [&](GameObjectId id) {
        const GameObject* o = scene.find(id);
        return !o || o->pendingDestroy();
    });
    return ids;
}

bool collidesAtPosition(
    const Scene2D& scene,
    GameObjectId movingObjectId,
    const glm::vec2& newPosition
) {
    const GameObject* movingObject = scene.findObject(movingObjectId);
    const ColliderData* movingCollider = movingObject ? getCollider(*movingObject) : nullptr;
    if (!movingCollider) {
        return false;
    }

    glm::vec2 world = newPosition;
    if (const GameObject* parent = scene.find(movingObject->parent())) {
        world = localToWorld(parent->world(), newPosition);
    }
    glm::vec2 movingCenter = world + movingCollider->offset;
    glm::vec2 movingHalfSize = movingCollider->size * 0.5f;

    AABB movingBox{};
    movingBox.min = movingCenter - movingHalfSize;
    movingBox.max = movingCenter + movingHalfSize;

    // shortcut: other objects' grid boxes can be one step stale, so the query box is inflated by this margin
    // and each candidate is re-tested against its live box. An object that moved more than the margin in one
    // step can be missed; raise the margin (or sync the grid on move) if something gets that fast.
    constexpr float kStaleMargin = 1.0f;
    AABB wide{movingBox.min - kStaleMargin, movingBox.max + kStaleMargin};

    for (GameObjectId id : queryAABB(scene, wide, QueryKind::Solid)) {
        const GameObject* other = scene.find(id);
        if (id == movingObjectId || !other->active || !hasCollider(*other) || other->collider->isTrigger) {
            continue;
        }
        if (intersects(movingBox, buildAABB(*other))) {
            return true;
        }
    }
    return false;
}

}
