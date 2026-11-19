#include "Collision.hpp"

#include <algorithm>

namespace {
// Segment ab passes through the open interior of the box (Liang-Barsky); lying on its edge does not count.
bool segmentCrossesBox(glm::vec2 a, glm::vec2 b, const Collision2D::AABB& box) {
    constexpr float eps = 1e-5f;
    const glm::vec2 d = b - a;
    float t0 = 0.0f, t1 = 1.0f;
    for (int axis = 0; axis < 2; ++axis) {
        const float lo = box.min[axis] + eps - a[axis], hi = box.max[axis] - eps - a[axis];
        if (d[axis] == 0.0f) {
            if (lo >= 0.0f || hi <= 0.0f) return false;
            continue;
        }
        float ta = lo / d[axis], tb = hi / d[axis];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
    }
    return t0 < t1;
}
}

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

    if (!solidBoxes(scene, movingObjectId, movingBox).empty()) {
        return true;
    }
    // shortcut: only segments crossing the box count, so a box wholly inside level geometry reads as free;
    // fine for the step-wise movers left (top-down), add a point-in-solid test if something can teleport in.
    std::vector<const Segment*> segments;
    scene.collision.query(movingBox, segments);
    for (const Segment* s : segments) {
        if (!s->oneWay && segmentCrossesBox(s->a, s->b, movingBox)) {
            return true;
        }
    }
    return false;
}

std::vector<AABB> solidBoxes(const Scene2D& scene, GameObjectId self, const AABB& area, const char* skipTag) {
    // shortcut: other objects' grid boxes can be one step stale, so the query box is inflated by this margin
    // and each candidate is re-tested against its live box. An object that moved more than the margin in one
    // step can be missed; raise the margin (or sync the grid on move) if something gets that fast.
    constexpr float kStaleMargin = 1.0f;
    AABB wide{area.min - kStaleMargin, area.max + kStaleMargin};

    std::vector<AABB> out;
    for (GameObjectId id : queryAABB(scene, wide, QueryKind::Solid)) {
        const GameObject* other = scene.find(id);
        if (id == self || !other->active || !hasCollider(*other) || other->collider->isTrigger ||
            (skipTag && other->hasTag(skipTag))) {
            continue;
        }
        const AABB box = buildAABB(*other);
        if (intersects(area, box)) {
            out.push_back(box);
        }
    }
    return out;
}

}
